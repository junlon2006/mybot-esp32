# mybot-esp32

[![CI](https://github.com/junlon2006/mybot-esp32/actions/workflows/ci.yml/badge.svg)](https://github.com/junlon2006/mybot-esp32/actions/workflows/ci.yml)
[![License](https://img.shields.io/github/license/junlon2006/mybot-esp32)](LICENSE)

**[English](README.md) | [简体中文](README.zh-CN.md)**

`mybot-esp32` is an independent ESP-IDF firmware project that brings the mybot AI voice-chat SDK
to ESP32-S3 devices. It owns board initialization, Wi-Fi provisioning, persistent storage, secure
HTTPS transport, audio capture/playback, input, display, and the firmware lifecycle around mybot.

The current development baseline is **ESP-IDF v5.5.2**. Supported board profiles are Zhengchen
1.54 TFT ML307 and M5Stack CoreS3.

> Project-maintained code is Apache-2.0 unless a file says otherwise. Bundled dependencies and media
> assets have separate terms. Read [License and dependencies](#license-and-dependencies) before
> redistributing source or firmware images.

## Features

- Native ESP-IDF component build of the pinned mybot SDK.
- Wi-Fi station reconnect and first-boot captive-portal provisioning.
- NVS persistence for device credentials and 0-100 speaker volume.
- HTTPS through `esp-tls` with the system CA bundle, SNI, and hostname verification.
- 16 kHz mono signed-16 PCM capture/playback with configurable 20/40/60 ms packet duration.
- Agora RTSA full-duplex audio, Cloud AEC, AI QoS, RTM channel subscription, and voice-print status.
- Chinese and English local pairing-code and Wi-Fi provisioning prompts.
- Compile-time board profiles with isolated Flash, PSRAM, partition, driver, and pin configuration.

## Supported Boards

| Board profile | Audio | Display and input | Storage |
| --- | --- | --- | --- |
| `zhengchen-1.54tft-ml307` | I2S microphone and speaker | ST7789, Boot and volume buttons | 16 MB QIO Flash, 8 MB Octal PSRAM |
| `m5stack-core-s3` | ES7210 and AW88298 | ILI9342 and FT6336 touch | 16 MB QIO Flash, 8 MB Quad PSRAM |

Both profiles currently use Wi-Fi networking. The ML307 UART is reserved by the Zhengchen hardware
profile, but the modem AT socket is not an lwIP network interface and is not supported by the RTC
transport.

## Build

Install ESP-IDF v5.5.2 by following the
[official setup guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/index.html),
then activate that installation in the current shell:

```sh
. /path/to/esp-idf/export.sh
test "$(idf.py --version)" = "ESP-IDF v5.5.2"
```

Use a separate build directory and generated sdkconfig for each board:

```sh
idf.py -B build/zhengchen-1.54tft-ml307 \
  -DMYBOT_BOARD=zhengchen-1.54tft-ml307 \
  -DSDKCONFIG=build/zhengchen-1.54tft-ml307/sdkconfig build

idf.py -B build/m5stack-core-s3 \
  -DMYBOT_BOARD=m5stack-core-s3 \
  -DSDKCONFIG=build/m5stack-core-s3/sdkconfig build
```

The default profile is `zhengchen-1.54tft-ml307`, but explicit board selection is recommended.
Board defaults supply the required Flash, PSRAM, and partition settings.

Flash and monitor the selected build:

```sh
idf.py -B build/zhengchen-1.54tft-ml307 -p /dev/ttyUSB0 flash monitor
```

## Provisioning and Controls

When NVS contains no Wi-Fi credentials, the device creates a configuration AP whose SSID starts
with `mybot-`. Connect to it and open `http://192.168.4.1`. mybot starts only after the station has
a usable IP address; the display shows `WIFI SETUP` while provisioning.

- Zhengchen: short-press Boot to start/stop a conversation; hold Boot for 3 seconds to provision.
- CoreS3: short-touch the screen to start/stop a conversation; hold for 3 seconds to provision.

A provisioning request stops mybot first. After Wi-Fi reconnects and obtains an IP address, the
firmware starts mybot again.

The default device-service endpoint is:

```text
https://mybot.sh2.agoralab.co/api
```

Use `idf.py -B <build-dir> menuconfig` and the `mybot` menu to select the language, endpoint, audio
packet duration, Cloud AEC, and AI QoS.

## Hardware Notes

### Zhengchen 1.54 TFT ML307

| Capability | Pins/configuration |
| --- | --- |
| Microphone I2S1 RX | WS GPIO4, BCLK GPIO5, DIN GPIO6 |
| Speaker I2S0 TX | DOUT GPIO7, BCLK GPIO15, WS GPIO16 |
| Buttons | Boot GPIO0, volume up GPIO10, volume down GPIO39 |
| ST7789 | MOSI GPIO41, SCLK GPIO42, CS GPIO21, DC GPIO40, RESET GPIO45 |
| Backlight / power hold | GPIO20 / GPIO2 high |
| ML307 UART (reserved) | ESP TX GPIO12, RX GPIO11 |

### M5Stack CoreS3

| Capability | Pins/configuration |
| --- | --- |
| Shared I2C1 | SDA GPIO12, SCL GPIO11 |
| ES7210/AW88298 I2S0 | MCLK GPIO0, BCLK GPIO34, WS GPIO33, DIN GPIO14, DOUT GPIO13 |
| ILI9342 SPI3 | MOSI GPIO37, SCLK GPIO36, CS GPIO3, DC GPIO35 |
| FT6336 / AXP2101 / AW9523 | I2C addresses `0x38` / `0x34` / `0x58` |

GPIO0 is audio MCLK on CoreS3 and is not used as a button. CoreS3 has no dedicated volume keys;
the firmware restores its persisted device volume at startup.

## Repository Layout

```text
components/agora_rtc/        ESP32-S3 Agora RTSA package
components/aosl/             AOSL with ESP32-S3 platform integration
components/mybot_sdk/        ESP-IDF build wrapper for the mybot SDK
components/mybot_platform/   Common services, reusable drivers, and board profiles
third_party/mybot/           Pinned mybot public headers and core sources
main/                        Firmware entry point and project Kconfig
```

## Validation and Limitations

CI builds both board profiles, both languages, and 20/40/60 ms audio packet durations where
applicable. M5Stack CoreS3 provisioning and bidirectional voice interaction have also been validated
on real hardware. A successful build is not a substitute for hardware validation on a release
device.

Known limitations:

- ML307/4G networking, local wake words, battery reporting, and low-power operation are not wired up.
- CoreS3 camera, battery reporting, and automatic sleep are not wired up.
- NVS encryption, Flash encryption, and Secure Boot belong to the product provisioning process.

## Documentation

- [Board porting](docs/BOARD_PORTING.md)
- [Contributing](CONTRIBUTING.md)
- [Support](SUPPORT.md)
- [Release checklist](docs/RELEASING.md)
- [Vendored dependency baselines](VENDORED_SOURCES.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)

## License and Dependencies

Project-maintained code is licensed under [Apache-2.0](LICENSE) unless a file carries another SPDX
identifier. The repository also contains dependencies and media assets under separate licenses or
distribution terms, including AOSL and Agora RTSA. The root license does not override those terms.
Review [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and every bundled license before use or
redistribution.
