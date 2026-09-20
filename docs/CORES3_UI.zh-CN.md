# CoreS3 LVGL UI

[English](CORES3_UI.md) | [简体中文](CORES3_UI.zh-CN.md)

`CONFIG_MYBOT_CORES3_LVGL_UI` 为 `m5stack-core-s3` 选择可选的 LVGL 显示后端，默认关闭。
普通固件保留现有缓存渲染器；LVGL 构建不编入该渲染器，也不分配原有 16 页、约 2.34 MiB
的 PSRAM 画面缓存。

## 显示内容

界面覆盖现有十种流程页面：启动、Wi-Fi 配网、Wi-Fi 断开、服务启动、配对、配对码、
就绪、对话、失败及停止，通过圆角状态卡和本地表情区分不同状态。对话过程中常驻显示
声纹注册状态，同时显示聆听/思考/说话状态。
静态文案跟随固件的中英文语言设置，配对码仍动态显示。

观察到配对成功、声纹注册或网络恢复的状态变化时，顶部会显示最多两秒的短时通知，不遮挡
配对数字，也不替换对话中的声纹指示。重复的相同状态不会反复触发通知，切换到其他流程
页面时会提前清除上一条通知。

聆听/思考/说话通过小范围点状或条状动画指示活动，最高 10 fps。这些动画表示 SDK 状态，
不测量实际音频幅度。其他页面保持静态，退出活动状态时停止动画。

UI 接收现有 SDK 公共 LCD 内容及板级配网状态，不增加聊天正文、云端情绪消息、GIF 动画、
电量显示或新的网络/SDK 协议。内置字库覆盖静态界面文案，不用于任意中文聊天文本。
短触屏幕仍开始/结束对话，长按屏幕三秒仍进入 Wi-Fi 配网。

## 主题与动画设置

开启 LVGL 后端后，可在 menuconfig 的 `mybot` 菜单调整：

| 配置 | 默认值 | 作用 |
| --- | --- | --- |
| `CONFIG_MYBOT_CORES3_UI_LIGHT_THEME` | `n` | 默认深色主题，设为 `y` 使用浅色主题 |
| `CONFIG_MYBOT_CORES3_UI_ANIMATIONS` | `y` | 开启状态活动动画，设为 `n` 保持静态指示 |

两项均在编译时确定，不增加切换主题或动画的新触摸手势。关闭状态动画仍保留状态切换、
本地表情和事件通知。

## 编译与烧录

使用 ESP-IDF v5.5.2，并创建独立构建目录和 sdkconfig：

```sh
idf.py -B build/cores3-lvgl-ui \
  -DMYBOT_BOARD=m5stack-core-s3 \
  -DSDKCONFIG=build/cores3-lvgl-ui/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;ci/lvgl-ui.defaults" build
idf.py -B build/cores3-lvgl-ui -p <PORT> flash monitor
```

默认使用中文、关闭视频、深色主题并开启状态动画。比较不同组合时，请使用独立目录：

| 组合 | 建议构建目录 | `SDKCONFIG_DEFAULTS` |
| --- | --- | --- |
| 中文、视频关闭 | `build/cores3-lvgl-ui` | `sdkconfig.defaults;ci/lvgl-ui.defaults` |
| 英文、视频关闭 | `build/cores3-lvgl-ui-en` | `sdkconfig.defaults;ci/lvgl-ui.defaults;ci/en-us.defaults` |
| 中文、视频开启 | `build/cores3-lvgl-ui-video` | `sdkconfig.defaults;ci/lvgl-ui.defaults;ci/video.defaults` |
| 英文、视频开启 | `build/cores3-lvgl-ui-video-en` | `sdkconfig.defaults;ci/lvgl-ui.defaults;ci/en-us.defaults;ci/video.defaults` |
| 英文、浅色主题、视频关闭 | `build/cores3-lvgl-ui-light-en` | `sdkconfig.defaults;ci/lvgl-ui.defaults;ci/lvgl-ui-light.defaults;ci/en-us.defaults` |
| 中文、静态指示、视频开启 | `build/cores3-lvgl-ui-static-video` | `sdkconfig.defaults;ci/lvgl-ui.defaults;ci/lvgl-ui-static.defaults;ci/video.defaults` |

将命令中的 `-B`、`-DSDKCONFIG` 同时改为选定目录，并使用对应的 defaults 列表。
此选项仅支持 CoreS3。对比原有 UI 时，在另一目录编译且不加入 `ci/lvgl-ui.defaults`；
修改 defaults 不会覆盖已生成的 sdkconfig，也可通过对应构建的 `menuconfig` 关闭该选项。
浅色/静态 defaults 文件仅修改对应选项，必须与 `ci/lvgl-ui.defaults` 一起使用。

## 渲染与资源

UI 任务运行于 Core 1，优先级为 1。LVGL 对象使用 PSRAM，SPI 使用单个 320 × 16 的
RGB565 内部 DMA 缓冲，共 10,240 字节。通过局部刷新更新变化区域，不再复制完整缓存页，
也不分配图像缓存或全屏页面缓存。

四个 64 × 64 本地表情预先解码为常量图像保存在 Flash，像素数据合计约 64 KiB。
运行时不需要 PNG/GIF 解码器或大幅解码图像缓存；只有小范围状态指示动画，表情图像本身
保持静态。

提交状态时只复制最新 LCD 内容并唤醒 UI 任务，由 50 ms 的 LVGL 定时器应用待更新内容，
快速连续变化的中间状态可能合并。SDK 回调不执行绘图或等待 SPI 传输，板级配网和 SDK
对话通过引用计数共用显示资源。

LVGL 自身需要任务、对象、字体和渲染开销，移除旧页面缓存不等于已经确认总内存或 CPU
消耗降低。音频和摄像头代码保持不变，同时运行的表现仍需测量；SDK 公共接口保持不变。

生命周期日志使用 `event=lcd` 和 `backend=lvgl`。原有 `ui_stats` 的绘制/刷新耗时描述
默认缓存渲染器，不适用于此后端。通过 CPU/堆统计、生命周期日志及实际显示效果
评估 LVGL 路径。

## 真机验收

基础 LVGL 状态界面已通过真机测试，后续新增的主题、表情、通知及状态动画仍需真机回归。
CI 保留已有全部配置，覆盖四种中英文/视频组合及浅色主题、静态指示变体；编译和主机检查
不能证明真实设备上的听音或显示效果。

用于发布前，请完成以下验证：

- 检查方向、红绿蓝颜色、小字、大号配对数字及全部十种页面。
- 验证声纹及聆听/思考/说话状态，包括快速切换和挂断。
- 验证明暗主题及动画开关，确认静态页面停止动画、事件通知两秒内或流程切换时消失，
  配对码和声纹状态始终可读。
- 验证短触及长按三秒配网、首次启动配网、Wi-Fi 断线恢复，以及反复 SDK 停止/启动，
  确认没有旧画面残留或黑屏。
- 界面更新时持续进行全双工语音，检查沙沙声；开启 1 fps 视频后重复测试，并在相同音量、
  网络下与原渲染器对比。
- 通过 `CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR` 记录每核 CPU、内部 RAM/PSRAM 峰值、
  最低剩余堆及最大连续空闲块，多轮会话确认内存没有持续增长。

每份结果记录固件语言、UI/视频/主题/动画配置、音量和完整串口日志。
