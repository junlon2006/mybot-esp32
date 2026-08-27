# mybot-esp32

mybot SDK 的 ESP32-S3 开源移植工程。当前开发基线为 ESP-IDF v5.5.2，首个目标板为
xiaozhi 的 `BOARD_TYPE_ZHENGCHEN_1_54TFT_ML307`。

## 当前能力

- mybot 1.0.0 core 以原生 ESP-IDF component 方式构建。
- 使用 xiaozhi 验证过的 ESP32-S3 Agora RTSA 1.10.0 和 AOSL。
- AOSL 包含 BK7258 移植验证过的 `aosl_ctor()` / `aosl_dtor()` 引用计数生命周期。
- Wi-Fi STA 自动重连和首次启动 AP 配网门户。
- NVS 持久化 mybot 设备凭据。
- `esp-tls`、系统 CA bundle、SNI 和 hostname 校验的 HTTPS transport。
- 征辰 1.54 TFT 板的 I2S0 扬声器、I2S1 麦克风、Boot/音量按键和 ST7789 状态屏。
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
```

当前默认 Board 为 `zhengchen-1.54tft-ml307`，但仍建议显式选择 Board 并使用隔离构建目录。
Board profile 会自动追加对应的 Flash、PSRAM 和分区默认配置。不同 Board 必须使用不同的
构建目录；新增 Board 的结构和约束见 [docs/BOARD_PORTING.md](docs/BOARD_PORTING.md)。

烧录和查看日志：

```sh
idf.py -B build/zhengchen-1.54tft-ml307 -p /dev/ttyUSB0 flash monitor
```

首次启动且 NVS 没有 Wi-Fi 凭据时，设备创建以 `mybot-` 开头的配置 AP。连接后打开
`http://192.168.4.1` 完成配网。网络连接是 mybot 启动的先决条件，设备拿到 IP 后才启动
mybot 服务；配网期间 ST7789 显示 `WIFI SETUP`。

运行时长按 Boot 键 3 秒会先停止 mybot，再进入 Wi-Fi 配网模式；重新拿到 IP 后自动启动
mybot。短按 Boot 键仍用于开始或结束对话。

服务端默认使用中国区：

```text
https://mybot.sh2.agoralab.co/api
```

可通过 `idf.py -B build/zhengchen-1.54tft-ml307 menuconfig` 的 `mybot` 菜单切换服务地址、
音频 ptime、Cloud AEC
和 AI QoS。

## 硬件

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

- 尚未完成真实硬件烧录、双向 RTC 音频和长期运行验证。
- ML307 网络、配对码语音播报、唤醒词、电池和低功耗尚未接入。
- NVS encryption、Flash encryption 和 Secure Boot 需在产品烧录流程中配置。
