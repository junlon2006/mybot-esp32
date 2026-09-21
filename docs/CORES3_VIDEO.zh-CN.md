# CoreS3 视频上行

[English](CORES3_VIDEO.md) | [简体中文](CORES3_VIDEO.zh-CN.md)

可选的 `m5stack-core-s3` 摄像头适配器采集 GC0308 QVGA YUYV 图像，软件编码完整 JPEG，
通过 mybot 公开视频回调上传。发送最多 1 fps，RTC 拒绝帧时也不会加速重试。传感器保留
20 fps 时序，平台每秒最多发起 5 次实际 DVP 采集，两次请求至少间隔 200 ms，同时只入队
一张缓冲。每帧完成后，空队列使 DMA 和 PSRAM 复制停止，直到下次采样；传感器曝光和
VSYNC 继续运行。只编码选中的样本，慢发送不会积压旧帧或触发补采。
适配器位于
[`cores3_camera_video.c`](../components/mybot_platform/src/drivers/video/cores3_camera_video.c)。

## 开启和构建

激活 ESP-IDF v5.5.2 环境后，在仓库根目录使用独立构建目录：

```sh
idf.py -B build/cores3-video \
  -DMYBOT_BOARD=m5stack-core-s3 \
  -DSDKCONFIG=build/cores3-video/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;ci/video.defaults" reconfigure build
idf.py -B build/cores3-video -p <PORT> flash monitor
```

已有 CoreS3 构建可在对应目录的 menuconfig 中开启 `mybot → Enable CoreS3 camera JPEG
uplink (1 fps)`。`CONFIG_MYBOT_ENABLE_VIDEO` 默认关闭；其他板型开启时会报配置错误。
CoreS3-SE 没有 GC0308，不属于视频支持范围。音频帧长保持当前支持的 60 ms。
全新构建默认中文；英文版本在新的构建目录中给 defaults 列表追加 `;ci/en-us.defaults`。
所有 CoreS3 构建均使用 LVGL，不需要单独开启 UI。
复用已有构建目录时，运行 `idf.py -B build/cores3-video reconfigure build`，让组件的隐藏 DMA
默认值重新生成；确认该目录的 `sdkconfig` 中为 `CONFIG_CAM_CTRL_DVP_DMA_BUFFER_SIZE=16384`。

## 硬件与内存

| 信号 | 接线 |
| --- | --- |
| SCCB | 共用原生 I2C1，SDA12/SCL11，地址 `0x21`，100 kHz |
| D0–D7 | GPIO39/40/41/42/15/16/48/47 |
| VSYNC/HREF/PCLK | GPIO46/38/45 |
| XCLK | 外部 20 MHz 晶振，不占用输出 GPIO |
| Reset | AW9523 P1_0；加锁读改写，只改变本设备对应位 |

两张 153,600 字节采集缓冲和一张 131,072 字节 JPEG 缓冲共约 428 KiB PSRAM，编码器还会
申请工作区。开启视频时，当前
[`CAM_CTRL_DVP_DMA_BUFFER_SIZE`](../components/esp_cam_sensor/Kconfig) 默认值为 16,384 字节。
对于 320×240 YUYV，DVP 驱动按整帧可分割的大小调整为两个 7,680 字节半缓冲，实际分配
15 KiB 内部 DMA 内存，另有描述符和 3 KiB 采集任务栈。上述数值对应当前默认配置，
对比旧固件时先核对其生成配置中的 DMA 大小；JPEG 任务栈为 6 KiB。
通话中应同时检查内部内存剩余和最大连续块，不能只看 PSRAM。LVGL 对象使用 PSRAM，UI
另有 10 KiB 内部 DMA 缓冲，不分配全屏页面缓存。
本次 DMA 比之前多占用 7.5 KiB 内部 RAM，描述符另计。两张 PSRAM 帧缓冲仍保留，轮流使用，
同时最多只有一张已提交给驱动。

JPEG 任务优先级为 3，不固定 CPU 核心，不额外创建编码辅助任务；DVP 拷贝任务保留上游
较高优先级以及时处理 DMA。摄像头不使用音频 I2S 外设。LCD 继续显示状态页，不提供摄像头预览。

## 生命周期与码率

初始化只准备资源，不启动 DVP 采集，也不调用发送回调。RTC 连接后启动工作任务；发送回调
返回前始终保留 JPEG 缓冲。每次出队最多等待 100 ms，超时继续等待原来的在途缓冲，不会
重复入队；一秒内持续无帧则打印错误并停止流。这也限制了底层对坏帧的自动重试时间，
5 fps 指正常采样，不代表每次失败的硬件重试都受此限制。停止最多等待 3 秒，让工作任务和在途
回调退出；超时后保留资源并返回失败，由 SDK 重试。确认停止后才销毁。摄像头销毁不会删除
Board 常驻 I2C 总线。

声明码率范围为 64–512 kbit/s，初始 288 kbit/s。JPEG 质量初始 60，收到带宽反馈后在
30–60 范围调整。超过当前一秒字节预算或固定输出缓冲大小的帧会丢弃，因此弱网或复杂画面
可能低于 1 fps。SDK 接受帧不代表服务端已经完成识别。

## 日志与验收

`cores3_video` 打印生命周期日志，推流时每 10 秒汇总 `event=video_stats`：窗口时长
`window_ms`、成功出队数 `captured`、编码数 `encoded`、接受数 `sent`、拒绝数 `rejected`、
丢帧数 `dropped`、采集错误 `capture_errors`、
上次图像大小 `last_bytes`、质量 `quality`、目标码率 `target_bps` 与最大编码耗时
`encode_max_us`。计数及 `encode_max_us` 每周期清零，`last_bytes`、`quality` 与
`target_bps` 保留最近值。`captured` 统计成功出队，而非每次 DMA 尝试；除以
`window_ms / 1000` 可计算实际收到的帧率。空队列期间未请求的帧不计入 `dropped`，
`sent` 表示 SDK/RTSA 接受发送。正常采集可接近每 10 秒 50 帧；受编码、发送耗时及 200 ms
采样间隔影响，上行可能约每 10 秒 8–10 帧，始终不超过 1 fps。

CoreS3 摄像头已完成真机调通；普通音频在视频开启和关闭两种场景下也已验证正常。
这是已有的硬件验证记录，本次采集调度、增大的 DMA 缓冲及 UI 变化仍需真机回归和 CPU 测量。

真机需验证 GC0308 识别、颜色与方向、服务端收到并识别图像、发送间隔、图像质量、CPU/内存
以及音频连续性。覆盖重复开始/结束对话、断网、长按触摸配网、带宽降低和摄像头缺失，确认
停止后无回调、多轮运行无内存持续增长。CI 覆盖中英文视频构建；真机和服务端验收另行记录。
对比测试应保持相同场景和音频活动，观察 `captured/window_ms`、每核 CPU、内部剩余堆和
最大连续块；DMA 流量的理论减少不等于已经测得 CPU 降幅。
