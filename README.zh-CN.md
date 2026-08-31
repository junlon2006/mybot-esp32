# mybot-esp32

[![CI](https://github.com/junlon2006/mybot-esp32/actions/workflows/ci.yml/badge.svg)](https://github.com/junlon2006/mybot-esp32/actions/workflows/ci.yml)
[![License](https://img.shields.io/github/license/junlon2006/mybot-esp32)](LICENSE)

**[English](README.md) | [简体中文](README.zh-CN.md)**

`mybot-esp32` 是一个独立的 ESP-IDF 固件工程，将 mybot AI 语音对话 SDK 运行在 ESP32-S3
设备上。工程负责板级初始化、Wi-Fi 配网、持久化存储、安全 HTTPS、音频采集与播放、输入、
显示，以及 mybot 外围的固件生命周期。

当前开发基线为 **ESP-IDF v5.5.2**，支持征辰 1.54 TFT ML307 与 M5Stack CoreS3。

> 除非文件另有声明，本工程自维护代码使用 Apache-2.0。仓库内依赖和媒体资源有各自的
> 许可条款；重新分发源码或固件前请阅读[许可证与依赖](#许可证与依赖)。

## 当前能力

- 固定版本 mybot SDK 以原生 ESP-IDF component 方式构建。
- Wi-Fi STA 自动重连与首次启动配网页面。
- NVS 持久化设备凭据和 0-100 扬声器音量。
- 通过 `esp-tls`、系统 CA bundle、SNI 与 hostname 校验实现 HTTPS。
- 16 kHz 单声道 signed-16 PCM，音频包长可配置为 20/40/60 ms。
- Agora RTSA 全双工音频、Cloud AEC、AI QoS、RTM 频道订阅与声纹状态显示。
- 中英文配对码与 Wi-Fi 配网本地提示音。
- 编译期 Board profile，隔离 Flash、PSRAM、分区、驱动和引脚配置。

## 支持的板卡

| Board profile | 音频 | 显示与输入 | 存储 |
| --- | --- | --- | --- |
| `zhengchen-1.54tft-ml307` | I2S 麦克风和扬声器 | ST7789、Boot 与音量按键 | 16 MB QIO Flash、8 MB Octal PSRAM |
| `m5stack-core-s3` | ES7210 与 AW88298 | ILI9342 与 FT6336 触摸 | 16 MB QIO Flash、8 MB Quad PSRAM |

两个 profile 当前均使用 Wi-Fi。征辰板预留 ML307 UART，但 Modem AT socket 不是 lwIP
网络接口，当前 RTC 传输不支持该网络路径。

## 构建

按照[官方安装指南](https://docs.espressif.com/projects/esp-idf/zh_CN/v5.5.2/esp32s3/get-started/index.html)
安装 ESP-IDF v5.5.2，并在当前 shell 中激活该安装目录：

```sh
. /path/to/esp-idf/export.sh
test "$(idf.py --version)" = "ESP-IDF v5.5.2"
```

每块板使用独立的构建目录和生成的 sdkconfig：

```sh
idf.py -B build/zhengchen-1.54tft-ml307 \
  -DMYBOT_BOARD=zhengchen-1.54tft-ml307 \
  -DSDKCONFIG=build/zhengchen-1.54tft-ml307/sdkconfig build

idf.py -B build/m5stack-core-s3 \
  -DMYBOT_BOARD=m5stack-core-s3 \
  -DSDKCONFIG=build/m5stack-core-s3/sdkconfig build
```

默认 profile 是 `zhengchen-1.54tft-ml307`，但建议始终显式选择 Board。Board defaults 会
提供所需的 Flash、PSRAM 和分区配置。

烧录并查看日志：

```sh
idf.py -B build/zhengchen-1.54tft-ml307 -p /dev/ttyUSB0 flash monitor
```

## 配网与控制

NVS 中没有 Wi-Fi 凭据时，设备创建以 `mybot-` 开头的配置 AP。连接后打开
`http://192.168.4.1` 完成配网。STA 获取可用 IP 后才启动 mybot；配网期间显示
`WIFI SETUP`。

- 征辰板：短按 Boot 开始/结束对话；长按 Boot 3 秒进入配网。
- CoreS3：短触屏幕开始/结束对话；长按屏幕 3 秒进入配网。

请求配网时固件会先停止 mybot；Wi-Fi 重新连接并获取 IP 后自动再次启动。

默认设备服务地址：

```text
https://mybot.sh2.agoralab.co/api
```

使用 `idf.py -B <build-dir> menuconfig` 的 `mybot` 菜单配置语言、服务地址、音频包长、
Cloud AEC 与 AI QoS。

## 硬件说明

### 征辰 1.54 TFT ML307

| 能力 | 引脚/配置 |
| --- | --- |
| 麦克风 I2S1 RX | WS GPIO4、BCLK GPIO5、DIN GPIO6 |
| 扬声器 I2S0 TX | DOUT GPIO7、BCLK GPIO15、WS GPIO16 |
| 按键 | Boot GPIO0、音量加 GPIO10、音量减 GPIO39 |
| ST7789 | MOSI GPIO41、SCLK GPIO42、CS GPIO21、DC GPIO40、RESET GPIO45 |
| 背光 / 电源保持 | GPIO20 / GPIO2 输出高 |
| ML307 UART（预留） | ESP TX GPIO12、RX GPIO11 |

### M5Stack CoreS3

| 能力 | 引脚/配置 |
| --- | --- |
| 公共 I2C1 | SDA GPIO12、SCL GPIO11 |
| ES7210/AW88298 I2S0 | MCLK GPIO0、BCLK GPIO34、WS GPIO33、DIN GPIO14、DOUT GPIO13 |
| ILI9342 SPI3 | MOSI GPIO37、SCLK GPIO36、CS GPIO3、DC GPIO35 |
| FT6336 / AXP2101 / AW9523 | I2C 地址 `0x38` / `0x34` / `0x58` |

GPIO0 是 CoreS3 的音频 MCLK，不作为按键使用。CoreS3 没有独立音量键，固件启动时恢复
持久化的设备音量。

## 目录结构

```text
components/agora_rtc/        ESP32-S3 Agora RTSA 包
components/aosl/             AOSL 与 ESP32-S3 平台集成
components/mybot_sdk/        mybot SDK 的 ESP-IDF 构建包装
components/mybot_platform/   公共服务、可复用驱动与 Board profile
third_party/mybot/           固定版本的 mybot 公共头和核心源码
main/                        固件入口与工程 Kconfig
```

## 验证与限制

CI 构建两种 Board profile、两种语言，以及适用的 20/40/60 ms 音频包长。M5Stack CoreS3
已完成真机配网与双向语音交互验证。编译成功不能替代发布硬件上的真实设备验证。

已知限制：

- ML307/4G 网络、本地唤醒词、电池状态与低功耗尚未接入。
- CoreS3 摄像头、电池状态与自动休眠尚未接入。
- NVS encryption、Flash encryption 与 Secure Boot 由产品烧录流程配置。

## 文档

- [板级移植](docs/BOARD_PORTING.zh-CN.md)
- [参与贡献](CONTRIBUTING.zh-CN.md)
- [支持](SUPPORT.zh-CN.md)
- [发布检查清单](docs/RELEASING.zh-CN.md)
- [固定依赖基线](VENDORED_SOURCES.md)
- [第三方声明](THIRD_PARTY_NOTICES.md)

## 许可证与依赖

除非文件带有其他 SPDX 标识，本工程自维护代码使用 [Apache-2.0](LICENSE)。仓库还包含
采用独立许可证或分发条款的依赖和媒体资源，包括 AOSL 与 Agora RTSA；根目录许可证不会
覆盖这些条款。使用或重新分发前请阅读 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
以及各组件随附的许可证。
