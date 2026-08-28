# mybot-esp32

mybot SDK 的 ESP32-S3 开源移植工程。当前开发基线为 ESP-IDF v5.5.2，支持 xiaozhi
硬件基线中的征辰 1.54 TFT ML307 和 M5Stack CoreS3。

## 当前能力

- mybot 1.0.0 core 以原生 ESP-IDF component 方式构建。
- 使用 xiaozhi 验证过的 ESP32-S3 Agora RTSA 1.10.0 和 AOSL。
- AOSL 包含 BK7258 移植验证过的 `aosl_ctor()` / `aosl_dtor()` 引用计数生命周期。
- Wi-Fi STA 自动重连和首次启动 AP 配网门户。
- NVS 持久化 mybot 设备凭据。
- `esp-tls`、系统 CA bundle、SNI 和 hostname 校验的 HTTPS transport。
- 征辰 1.54 TFT 板的 I2S0 扬声器、I2S1 麦克风、Boot/音量按键和 ST7789 状态屏。
- M5Stack CoreS3 的 ES7210 麦克风、AW88298 扬声器、ILI9342 状态屏和 FT6336 触摸输入。
- 0-100 扬声器音量控制，默认值为 70 并通过 NVS 持久化；征辰板使用平方软件增益和
  音量按键，CoreS3 使用 AW88298 硬件音量。
- 中英文配对码语音播报，使用内嵌 Ogg/Opus 资源并在运行时解码到 PSRAM。
- 自动或用户请求进入配网模式时，播放所选语言的本地配网提示音。
- 16 kHz、单声道、signed 16-bit PCM，Cloud AEC 默认开启。

板名包含 ML307，但 xiaozhi 对该板的首次启动默认同样选择 Wi-Fi。当前 Agora/AOSL socket
后端依赖 lwIP，因此首个版本固定使用 Wi-Fi；ML307 AT socket 没有注册为 lwIP netif，4G RTC
不在当前支持范围内。

## 构建

```sh
get_idf
idf.py --version                 # 必须为 ESP-IDF v5.5.2
idf.py -B build/zhengchen-1.54tft-ml307 \
  -DMYBOT_BOARD=zhengchen-1.54tft-ml307 \
  -DSDKCONFIG=build/zhengchen-1.54tft-ml307/sdkconfig build

idf.py -B build/m5stack-core-s3 \
  -DMYBOT_BOARD=m5stack-core-s3 \
  -DSDKCONFIG=build/m5stack-core-s3/sdkconfig build
```

当前默认 Board 为 `zhengchen-1.54tft-ml307`，但仍建议显式选择 Board 并使用隔离构建目录。
Board profile 会自动追加对应的 Flash、PSRAM 和分区默认配置。不同 Board 必须使用不同的
构建目录；新增 Board 的结构和约束见 [docs/BOARD_PORTING.md](docs/BOARD_PORTING.md)。

烧录和查看日志（将构建目录替换为目标 Board）：

```sh
idf.py -B build/zhengchen-1.54tft-ml307 -p /dev/ttyUSB0 flash monitor
```

首次启动且 NVS 没有 Wi-Fi 凭据时，设备创建以 `mybot-` 开头的配置 AP。连接后打开
`http://192.168.4.1` 完成配网。网络连接是 mybot 启动的先决条件，设备拿到 IP 后才启动
mybot 服务；配网期间状态屏显示 `WIFI SETUP`。

征辰板运行时长按 Boot 键 3 秒会请求配网，短按 Boot 键开始或结束对话。CoreS3 短触
屏幕开始或结束对话，长按屏幕 3 秒请求配网。两块板都会先停止 mybot，再进入配网；
重新拿到 IP 后自动启动 mybot。

服务端默认使用中国区：

```text
https://mybot.sh2.agoralab.co/api
```

可通过 `idf.py -B build/zhengchen-1.54tft-ml307 menuconfig` 的 `mybot` 菜单切换中英文、服务地址、音频 ptime、Cloud AEC
和 AI QoS。

## 硬件

### 征辰 1.54 TFT ML307

| 能力 | 引脚/配置 |
| --- | --- |
| 麦克风 I2S1 RX | WS GPIO4、BCLK GPIO5、DIN GPIO6 |
| 扬声器 I2S0 TX | DOUT GPIO7、BCLK GPIO15、WS GPIO16 |
| 按键 | Boot GPIO0、音量加 GPIO10、音量减 GPIO39 |
| ST7789 | MOSI GPIO41、SCLK GPIO42、CS GPIO21、DC GPIO40、RESET GPIO45 |
| 背光 | GPIO20 |
| 电源保持 | GPIO2 输出高 |
| ML307（预留） | ESP TX GPIO12、RX GPIO11 |

目标模组配置按 xiaozhi 基线固定为 16 MB QIO Flash、8 MB Octal PSRAM、240 MHz CPU。

### M5Stack CoreS3

| 能力 | 引脚/配置 |
| --- | --- |
| 公共 I2C1 | SDA GPIO12、SCL GPIO11 |
| ES7210/AW88298 I2S0 | MCLK GPIO0、BCLK GPIO34、WS GPIO33、DIN GPIO14、DOUT GPIO13 |
| ILI9342 SPI3 | MOSI GPIO37、SCLK GPIO36、CS GPIO3、DC GPIO35 |
| FT6336 触摸 | I2C 地址 `0x38`，20 ms 轮询 |
| 电源与背光 | AXP2101，I2C 地址 `0x34` |
| LCD/功放复位 | AW9523，I2C 地址 `0x58` |

CoreS3 profile 固定为 16 MB QIO Flash、8 MB Quad PSRAM、240 MHz CPU。GPIO0 是音频
MCLK，不作为按键使用。CoreS3 没有独立音量键，首版使用持久化设备音量。

## 目录

```text
components/aosl/             xiaozhi AOSL + 引用计数补丁
components/agora_rtc/        ESP32-S3 Agora RTSA
components/mybot_sdk/        mybot 到 ESP-IDF 的构建包装
components/mybot_platform/   ESP-IDF common、可复用驱动和 Board profile
third_party/mybot/           固定版本的 mybot 公共头与 core 源码
main/                        固件入口和 Kconfig
```

第三方来源和固定版本见 [VENDORED_SOURCES.md](VENDORED_SOURCES.md)，贡献和发布要求分别见
[CONTRIBUTING.md](CONTRIBUTING.md) 与 [docs/RELEASING.md](docs/RELEASING.md)。

## 已知限制

- M5Stack CoreS3 尚未完成真实硬件烧录、双向 RTC 音频和长期运行验证。
- CoreS3 摄像头、电池状态和自动休眠尚未接入。
- ML307 网络、唤醒词、电池和低功耗尚未接入。
- NVS encryption、Flash encryption 和 Secure Boot 需在产品烧录流程中配置。
