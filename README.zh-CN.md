# mybot-esp32

[![CI](https://github.com/junlon2006/mybot-esp32/actions/workflows/ci.yml/badge.svg)](https://github.com/junlon2006/mybot-esp32/actions/workflows/ci.yml)
[![License](https://img.shields.io/github/license/junlon2006/mybot-esp32)](LICENSE)

**[English](README.md) | [简体中文](README.zh-CN.md)**

`mybot-esp32` 是一个独立的 ESP-IDF 固件工程，将
[mybot AI 语音对话 SDK](https://github.com/junlon2006/mybot) 运行在 ESP32-S3 设备上。工程
负责板级初始化、Wi-Fi 配网、持久化存储、安全 HTTPS、音频采集与播放、输入、显示，以及
mybot 外围的固件生命周期。

当前开发基线为 **ESP-IDF v5.5.2**，支持征辰 1.54 TFT ML307 与 Wi-Fi 版本、M5Stack
CoreS3、M5Stack StickS3、搭配 XIAO ESP32S3 的 ReSpeaker Flex XVF3800 Circular-4，以及
SenseCAP Watcher。

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
| `zhengchen-1.54tft-wifi` | I2S 麦克风和扬声器 | ST7789、Boot 与音量按键 | 16 MB QIO Flash、80 MHz Octal PSRAM |
| `m5stack-core-s3` | ES7210 与 AW88298 | ILI9342 与 FT6336 触摸 | 16 MB QIO Flash、8 MB Quad PSRAM |
| `m5stack-stick-s3` | ES8311 | ST7789P3 与主按键 | 8 MB QIO Flash、8 MB Octal PSRAM |
| `respeaker-flex-xvf3800-circular4-xiao` | XVF3800 与 AIC3104 | XIAO Boot 与 XVF 板载按键；无显示 | 8 MB Flash、8 MB Octal PSRAM |
| `sensecap-watcher` | ES8311 与 ES7243E | SPD2010 与旋转编码器 | 32 MB QIO Flash、Octal PSRAM |

所有 profile 当前均使用 Wi-Fi。征辰 ML307 profile 预留 Modem UART，但 Modem AT socket
不是 lwIP 网络接口，当前 RTC 传输不支持该网络路径。Wi-Fi profile 不配置也不访问 Modem
UART 使用的 GPIO11 与 GPIO12。

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

idf.py -B build/zhengchen-1.54tft-wifi \
  -DMYBOT_BOARD=zhengchen-1.54tft-wifi \
  -DSDKCONFIG=build/zhengchen-1.54tft-wifi/sdkconfig build

idf.py -B build/m5stack-core-s3 \
  -DMYBOT_BOARD=m5stack-core-s3 \
  -DSDKCONFIG=build/m5stack-core-s3/sdkconfig build

idf.py -B build/m5stack-stick-s3 \
  -DMYBOT_BOARD=m5stack-stick-s3 \
  -DSDKCONFIG=build/m5stack-stick-s3/sdkconfig build

idf.py -B build/respeaker-flex-xvf3800-circular4-xiao \
  -DMYBOT_BOARD=respeaker-flex-xvf3800-circular4-xiao \
  -DSDKCONFIG=build/respeaker-flex-xvf3800-circular4-xiao/sdkconfig build

idf.py -B build/sensecap-watcher \
  -DMYBOT_BOARD=sensecap-watcher \
  -DSDKCONFIG=build/sensecap-watcher/sdkconfig build
```

默认 profile 是 `zhengchen-1.54tft-ml307`，但建议始终显式选择 Board。Board defaults 会
提供所需的 Flash、PSRAM 和分区配置。

烧录并查看日志：

```sh
idf.py -B build/zhengchen-1.54tft-ml307 -p /dev/tty.wchusbserial1430 flash monitor
```

首次烧录 SenseCAP Watcher 前，先备份其 200 KiB 出厂数据分区：

```sh
python -m esptool --chip esp32s3 --baud 2000000 --before default_reset \
  --after hard_reset --no-stub read_flash 0x9000 204800 nvsfactory.bin
```

必须使用 `sensecap-watcher` profile 构建并通过 `idf.py flash` 烧录；不要对该设备执行
`erase-flash`。擦除或替换 `nvsfactory` 区域可能永久丢失出厂标识和服务恢复数据。

## 配网与控制

NVS 中没有 Wi-Fi 凭据时，设备创建以 `mybot-` 开头的配置 AP。连接后打开
`http://192.168.4.1` 完成配网。STA 获取可用 IP 后才启动 mybot；配备显示屏的板卡在配网
期间显示 `WIFI SETUP`。

- 征辰 ML307 与 Wi-Fi：短按 Boot 开始/结束对话；长按 Boot 3 秒进入配网；音量按键调节并
  持久化扬声器音量。
- CoreS3：短触屏幕开始/结束对话；长按屏幕 3 秒进入配网。
- StickS3：短按主按键开始/结束对话；长按 3 秒进入配网。
- ReSpeaker Flex：短按 XIAO Boot 或 XVF 板载按键（GPI0/X1D09）开始/结束对话；长按任一
  按键 3 秒进入配网。
- SenseCAP Watcher：旋转编码器调节音量，短按开始/结束对话，长按 3 秒进入配网。

请求配网时固件会先停止 mybot；Wi-Fi 重新连接并获取 IP 后自动再次启动。

默认设备服务地址：

```text
https://mybot.sh2.agoralab.co/api
```

使用 `idf.py -B <build-dir> menuconfig` 的 `mybot` 菜单配置语言、服务地址、音频包长、
Cloud AEC 与 AI QoS。

## 硬件说明

### 征辰 1.54 TFT ML307 与 Wi-Fi

| 能力 | 引脚/配置 |
| --- | --- |
| 麦克风 I2S1 RX | WS GPIO4、BCLK GPIO5、DIN GPIO6 |
| 扬声器 I2S0 TX | DOUT GPIO7、BCLK GPIO15、WS GPIO16 |
| 按键 | Boot GPIO0、音量加 GPIO10、音量减 GPIO39 |
| ST7789 | MOSI GPIO41、SCLK GPIO42、CS GPIO21、DC GPIO40、RESET GPIO45 |
| 背光 / 电源保持 | GPIO20 / GPIO2 输出高 |
| ML307 UART（仅 `zhengchen-1.54tft-ml307`） | ESP TX GPIO12、RX GPIO11 |

两个 profile 共用显示、音频、按键和 GPIO2 电源保持实现。Wi-Fi profile 不配置 GPIO11 与
GPIO12；其 defaults 使用 16 MB QIO Flash 和 80 MHz Octal PSRAM，发布前需通过启动日志
确认实际 PSRAM 容量。

mybot 边界为 16 kHz 单声道 signed-16 PCM；首版物理 I2S 使用 16 kHz 单声道 32-bit left
slot。板卡原始扬声器配置使用 24 kHz 输出，因此必须通过真机确认播放速度、音调、稳定性和
双向音频。如果硬件必须使用 24 kHz 输出，应保持 SDK 侧 16 kHz 边界，并在 Board 音频驱动
内加入有状态重采样。

首版 Wi-Fi profile 不包含 GPIO9 充电状态、GPIO8 电池 ADC、温度监测、自动休眠和低功耗
能力。

### M5Stack CoreS3

| 能力 | 引脚/配置 |
| --- | --- |
| 公共 I2C1 | SDA GPIO12、SCL GPIO11 |
| ES7210/AW88298 I2S0 | MCLK GPIO0、BCLK GPIO34、WS GPIO33、DIN GPIO14、DOUT GPIO13 |
| ILI9342 SPI3 | MOSI GPIO37、SCLK GPIO36、CS GPIO3、DC GPIO35 |
| FT6336 / AXP2101 / AW9523 | I2C 地址 `0x38` / `0x34` / `0x58` |

GPIO0 是 CoreS3 的音频 MCLK，不作为按键使用。CoreS3 没有独立音量键，固件启动时恢复
持久化的设备音量。

### M5Stack StickS3

| 能力 | 引脚/配置 |
| --- | --- |
| 公共 I2C0 | SDA GPIO47、SCL GPIO48；M5PM1 地址 `0x6e`、ES8311 地址 `0x18` |
| ES8311 I2S0 | MCLK GPIO18、BCLK GPIO17、WS GPIO15、DIN GPIO16、DOUT GPIO14 |
| ST7789P3 SPI3 | MOSI GPIO39、SCLK GPIO40、CS GPIO41、DC GPIO45、RESET GPIO21、背光 GPIO38 |
| 输入 | 主按键 GPIO11，低电平有效 |

M5PM1 G2 同时为显示屏和 codec 供电，在固件运行期间保持开启；G3 仅在播放期间开启扬声器
功放。显示区域为 135 x 240，偏移为 (52, 40)。首版不使用 GPIO12、IMU、红外、电池状态、
关机手势和低功耗能力。

### ReSpeaker Flex XVF3800 Circular-4 与 XIAO ESP32S3

此 profile 面向搭配 XIAO ESP32S3 的
[Seeed ReSpeaker Flex Circular-4](https://wiki.seeedstudio.com/cn/respeaker_flex_introduction/)。

| 能力 | 引脚/配置 |
| --- | --- |
| XVF3800 I2S0 | BCLK GPIO8、WS GPIO7、DOUT GPIO44、DIN GPIO43 |
| 公共 I2C0 | SDA GPIO5、SCL GPIO6、400 kHz |
| XVF3800 / AIC3104 | I2C 地址 `0x2c` / `0x18` |
| 按键 | XIAO Boot GPIO0；XVF 板载按键 GPI0/X1D09 通过 I2C resource 36 读取 |

此 profile 能够采集或播放音频前，必须单独为 XVF3800 烧录 Circular-4 **16 kHz 双通道
I2S 固件**。ESP32-S3 工作在 I2S 从机模式。GPIO43 与 GPIO44 由 I2S 占用，控制台应使用
USB Serial/JTAG。声学处理由 XVF3800 完成，因此该 profile 禁用 Cloud AEC，避免重复 AEC。

### SenseCAP Watcher

| 能力 | 引脚/配置 |
| --- | --- |
| ES8311/ES7243E I2S0 | MCLK GPIO10、BCLK GPIO11、WS GPIO12、DIN GPIO15、DOUT GPIO16 |
| 公共 I2C0 | SDA GPIO47、SCL GPIO48；TCA9555 地址 `0x21` |
| SPD2010 QSPI | CLK GPIO7、D0 GPIO9、D1 GPIO1、D2 GPIO14、D3 GPIO13、CS GPIO45 |
| 输入 / 背光 | 编码器 GPIO41/GPIO42、TCA9555 P0.3 编码器按键、背光 GPIO8 |

此 profile 直接以 mybot 的 16 kHz 单声道 signed-16 边界驱动 codec。TCA9555 管理系统、LCD
与 codec 功放的上电时序。专用 32 MB 分区表保留出厂数据区，并提供两个 4 MB OTA slot。
首版不包含摄像头、触摸、LED、电池状态、关机手势和自动休眠。

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

CI 构建全部 Board profile、两种语言，以及适用的 20/40/60 ms 音频包长。M5Stack CoreS3
已完成真机配网与双向语音交互验证。征辰 Wi-Fi、M5Stack StickS3、ReSpeaker Flex 与
SenseCAP Watcher profile 尚未完成真机验证；编译成功不能替代发布硬件上的真实设备验证。

已知限制：

- ML307/4G 网络与本地唤醒词尚未接入。
- 征辰板充电状态、电池 ADC、温度监测、自动休眠与低功耗尚未接入；Wi-Fi profile 的实际
  PSRAM 容量尚未确认。
- CoreS3 摄像头、电池状态与自动休眠尚未接入。
- StickS3 GPIO12、IMU、红外、电池状态、关机手势与低功耗尚未接入。
- ReSpeaker Flex 当前仅支持 Circular-4，并需要单独烧录 XVF3800 16 kHz I2S 固件；尚未
  支持 Linear-4、XVF3800 固件升级、LED 环状态显示与 LCD 输出。
- SenseCAP Watcher 尚未支持摄像头、触摸、LED、电池状态、关机与低功耗；烧录时必须保留
  出厂数据分区。
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
