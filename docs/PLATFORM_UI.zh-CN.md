# 平台 LVGL UI

[English](PLATFORM_UI.md) | [简体中文](PLATFORM_UI.zh-CN.md)

所有已支持的带屏板型统一使用共享 LVGL 流程界面。板级配置自动设置隐藏的
`CONFIG_MYBOT_LVGL_UI`，不再提供渲染器选择开关或旧渲染器。
SDK LCD 接口、音频、摄像头及触摸/按键归属均不改变。

## 当前适配器

| Board | 面板与逻辑尺寸 | LVGL 适配器 | 输入行为 |
| --- | --- | --- | --- |
| `zhengchen-1.54tft-ml307` | ST7789，240 × 240 | 通用 ST7789 | 原有按键 |
| `zhengchen-1.54tft-wifi` | ST7789，240 × 240 | 通用 ST7789 | 原有按键 |
| `m5stack-core-s3` | ILI9342，320 × 240 | CoreS3 适配器 | 原有 FT6336 手势 |
| `m5stack-stick-s3` | ST7789P3，135 × 240 | 通用 ST7789 | 原有主按键 |
| `esp-vocat` | ST77916，360 × 360 | 通用面板适配器 | 原有 CST816S/Boot 路径 |
| `esp32-s3-touch-amoled-1.75` | CO5300，466 × 466 | 通用面板适配器 | 原有触摸/按键路径 |
| `esp32-s3-touch-amoled-1.75c` | CO5300，466 × 466 | 通用面板适配器 | 原有触摸/按键路径 |
| `sensecap-watcher` | SPD2010，412 × 412 | 通用面板适配器 | 原有旋钮路径 |

ReSpeaker Flex 没有 LCD，不编入 LVGL。ML307、Wi-Fi 及音视频协议与显示后端相互独立。

## 共享行为

界面接收 SDK 语义 LCD 内容：十种流程页面、配对码、声纹指示以及聆听/思考/说话状态。
坐标根据各面板逻辑宽高计算，不假设 CoreS3 的 320 × 240。少量本地状态图像预解码到
Flash，不加入运行时 PNG/GIF 解码器或全屏缓存。

配网页面中央显示实际设备 SoftAP 名称（`mybot-xxxx`），底部保留中英文连接热点提示。
标题与提示仅在超出标签宽度时循环滚动，短文字静止，退出配网后停止滚动。
`CONFIG_MYBOT_LVGL_UI_ANIMATIONS` 只控制状态活动动画，关闭后仍允许必要的文字滚动。

触摸和按键事件仍由各板输入驱动维护。UI 不增加手势，不改变配网或音视频生命周期。
后端使用一条局部 RGB565 DMA 条带和独立 LVGL 任务；面板适配器负责复位、偏移、颜色顺序、
背光和销毁。

## 编译示例

不同板型和语言请使用独立构建目录，无需额外启用 UI：

```sh
idf.py -B build/esp-vocat-lvgl \
  -DMYBOT_BOARD=esp-vocat \
  -DSDKCONFIG=build/esp-vocat-lvgl/sdkconfig \
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults build
```

主题和活动动画仍为编译选项，不新增运行时主题手势。浅色主题可追加
`ci/lvgl-ui-light.defaults`，静态状态指示可追加 `ci/lvgl-ui-static.defaults`。
修改 defaults 时请使用新的 sdkconfig，defaults 不会覆盖已有 sdkconfig 中保存的值。

## 验证状态

此前的 CoreS3 LVGL UI 已通过真机测试，配网热点名称显示及滚动仍需真机验证。
CI 覆盖九种板型的中英文、CoreS3 视频中英文，以及两种
主题/动画变体，共 22 种配置。这些是构建检查，不代表任何新增板型已经通过真机颜色、
触摸、音频、配网或销毁验证。真机验收需覆盖所有流程页、方向与颜色、触摸/按键、Wi-Fi
恢复、反复 mybot 停止/启动、全双工音频及 CPU/内存测量。
