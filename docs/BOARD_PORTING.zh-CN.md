# 板级移植

> [English](BOARD_PORTING.md) | [简体中文](BOARD_PORTING.zh-CN.md)

mybot 固件在编译期只选择一块 Board。工程不做运行时板卡探测，因为 ESP-IDF target、Flash
大小与模式、PSRAM 模式和分区表都必须在组件配置前确定。

## 目录布局

```text
components/mybot_platform/
  include/mybot_board.h       Board 元数据与注册入口
  src/common/                 与 Board 无关的 ESP-IDF 服务
  src/drivers/                可复用的硬件驱动
  src/core/                   所选 Board 的注册逻辑
  boards/<board-id>/          Board 描述、引脚、源码和 sdkconfig defaults
```

固定版本的 mybot core 保持与板卡无关。每个 Board 提供生命周期覆盖整个进程的
`mybot_board_t`，负责产品网络前置条件和配网流程，并注册一个完整的
`mybot_platform_descriptor_t`。mybot Wi-Fi adapter 只连接已可用的网络并监听运行期链路变化；
它不负责首次配网，也不关闭 Board 网络。

KV、按键、采集与播放是 SDK 必需能力；硬件音量、HTTPS、LCD、提示音与唤醒词在当前产品
配置要求时才是必需能力。

## 构建 Profile

根目录 CMake cache 变量 `MYBOT_BOARD` 在 `project()` 配置 ESP-IDF 前选择 profile。Profile
定义：

- `MYBOT_BOARD_TARGET`
- `MYBOT_BOARD_SDKCONFIG_DEFAULTS`
- `MYBOT_BOARD_INCLUDE_DIR`
- `MYBOT_BOARD_REQUIRED_CONFIGS`
- `MYBOT_BOARD_FORBIDDEN_CONFIGS`（可选）
- `MYBOT_BOARD_PARTITION_TABLE`
- `MYBOT_BOARD_SOURCES`
- `MYBOT_BOARD_REQUIRES`

每块 Board 必须使用独立构建目录与 sdkconfig。构建 cache 会拒绝原地切换
`MYBOT_BOARD`；组件配置也会拒绝不满足所选 profile 的旧 Flash、PSRAM 或分区设置。

即使两块 Board 使用同一个 ESP-IDF target，也必须隔离构建。例如
`zhengchen-1.54tft-ml307` 使用 Octal PSRAM，而 `m5stack-core-s3` 使用 Quad PSRAM。

```sh
idf.py -B build/<board-id> \
  -DMYBOT_BOARD=<board-id> \
  -DSDKCONFIG=build/<board-id>/sdkconfig build
```

Board defaults 管理 Flash、PSRAM 与分区设置；产品公共设置放在 `sdkconfig.defaults`，target
公共设置放在 `sdkconfig.defaults.<target>`。

## 新增 Board

1. 新增 `boards/<board-id>/board.cmake`、`board.c`、`board_config.h` 与
   `sdkconfig.defaults`。
2. 在 `boards/boards.cmake` 添加 Board ID 与 profile 映射。Board 选择不进入 menuconfig；
   `-DMYBOT_BOARD=<board-id>` 是唯一用户入口。
3. 硬件契约一致时复用 common service 与现有类型化 driver。
4. 实现 Board 网络生命周期：`ensure_network()` 只在 IP 网络可用后返回；
   `provision_wifi()` 只在配网后 STA 重新连接时返回；`shutdown_network()` 完成最终网络拆除。
5. 电源时序及特殊 codec、显示、触摸或输入行为应留在 Board 或硬件 driver 内，不要向
   `third_party/mybot` 添加 ESP-IDF 细节。
6. 保持 SDK 音频边界：16 kHz、单声道、signed 16-bit PCM，接口传帧数而不是字节数。
7. 增加隔离的 CI 构建与尺寸报告，并记录真实设备的配网、HTTPS、RTC、双向音频、输入、
   显示、挂断与重复启停验证。

LCD `indicators` 是语义化基础画面上的非互斥叠加状态。实现应显示已识别的 bit，不替换基础
流程标题，并忽略未知 bit。`MYBOT_LCD_INDICATOR_VP_REGISTERED` 当前只在
`MYBOT_LCD_SCREEN_IN_CONVERSATION` 有意义。ESP32-S3 显示实现会在尚未出现 registered
indicator 时显示声纹注册中，收到该 indicator 后切换到注册完成状态。

采集与播放共用一个物理 I2S 外设时，Board driver 应通过引用计数管理外设和 codec 状态。
SDK 会分别初始化和销毁采集与播放；配网提示音也可能在 SDK 首次启动前打开播放设备。
