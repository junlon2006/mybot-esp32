# Vendored Dependency Baselines

This file records only the revisions needed to reproduce the firmware build. License and
redistribution terms are documented separately in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

| Path | Source | Pinned revision |
| --- | --- | --- |
| `third_party/mybot` | `github.com/junlon2006/mybot` | v1.0.0 + Unreleased, commit `27324e7177b52ad9d8743aba31acf94d0a125f44` |
| `components/aosl` | `github.com/AgoraIO-Community/aosl` | v1.0.4 + post-release fixes, commit `84e086084ebcd0ae2455a0ce5721950c5fe2e656` |
| `components/agora_rtc` | Agora RTSA Lite package archive | v1.10.1, build 1270872 (`20260828_194128`) |
| `components/esp-wifi-connect` | `github.com/78/esp-wifi-connect` | 3.2.2, commit `c24b97c194e6b4a1d7be0237b3c28980661cac1e` |
| `components/esp_audio_codec` | ESP Component Registry `espressif/esp_audio_codec` | 2.5.0, commit `3bb83597d07b604e1ab5b78dd4370a28d6fa802d` |
| `components/esp_codec_dev` | `github.com/espressif/esp-adf` | 1.5.11, commit `73befa9ebffdd6e5065b7145329f115910e13ab5` |
| `components/esp_lcd_ili9341` | `github.com/espressif/esp-bsp` | 2.0.2, commit `fc8bd325efcdef6d5802554659debba303058af6` |
| `components/esp_lcd_spd2010` | ESP Component Registry `espressif/esp_lcd_spd2010` | 2.0.0~1, commit `12f6ca1182ec48889b17ec570fadaaf267cb336e` |
| `components/esp_lcd_co5300` | ESP Component Registry `espressif/esp_lcd_co5300` | 2.1.0, commit `56f3c5620464c061e968d3358bc897c528e097ee`, content hash `21f84c5c825509ebacbddb379555b49092ae165b3e19f0edfbf6f8eff8aa036d` |
| `components/esp_lcd_touch` | ESP Component Registry `espressif/esp_lcd_touch` | 1.2.1, commit `c927778a85eed239dd403c1719d4f543ad56e693`, content hash `3f85a7d95af876f1a6ecca8eb90a81614890d0f03a038390804e5a77e2caf862` |
| `components/esp_lcd_touch_cst9217` | ESP Component Registry `waveshare/esp_lcd_touch_cst9217` | 1.0.4, commit `43e6e696bbf0cf8671a97fcfe02c4c720e4e4d27`, content hash `27a00845832b7987cacf4c4125cbed5415d940987583065b01870ac080930856` |
| `components/esp_io_expander` | ESP Component Registry `espressif/esp_io_expander` | 1.2.1, commit `eb76dc6ecf21ccc4ee7ee58bfea3d3d31fa090cf` |
| `components/esp_io_expander_tca95xx_16bit` | ESP Component Registry `espressif/esp_io_expander_tca95xx_16bit` | 2.0.2, commit `53f6127ba3a1dd80fbdf9a76b759ccd1a8dc0101` |
| `components/button` | ESP Component Registry `espressif/button` | 4.2.0, commit `5f9cb98ae4d0e8153c4b4d1accf471214e5b6fe8` |
| `components/knob` | ESP Component Registry `espressif/knob` | 1.1.0, commit `5f9cb98ae4d0e8153c4b4d1accf471214e5b6fe8` |
| `components/cmake_utilities` | ESP Component Registry `espressif/cmake_utilities` | 0.5.0 |
| `components/m5pm1` | ESP Component Registry `m5stack/m5pm1` | 1.0.7, content hash `731f79d0629e245787440f5419aac5d7a82befeb25f97689d6a2d0331a24a72d` |
| `components/mybot_platform/assets` | `github.com/junlon2006/mybot-bk7258` | commit `2577b5977a9f137855a7acf1fcdcd4040c5db2ea` |

External implementation references and hardware-mapping verification baselines used for
project-maintained board ports are pinned separately; their application layer and dependency set
are not vendored into this repository.

| Paths | Source | Pinned revision |
| --- | --- | --- |
| `components/mybot_platform/boards/respeaker-flex-xvf3800-circular4-xiao`, `components/mybot_platform/src/drivers/audio/xvf3800_audio.c`, `partitions/v2/8m.csv` | `github.com/qiuyanli1990/respeaker-flex-circle-Agora-mybot` | commit `b06024382eb104c998aead4841e1df647193065b` |
| `components/mybot_platform/boards/sensecap-watcher`, `components/mybot_platform/src/drivers/audio/sensecap_codec_audio.c`, `components/mybot_platform/src/drivers/display/spd2010_lcd.c`, `partitions/v2/32m-sensecap.csv` | `github.com/junlon2006/xiaozhi-esp32` | commit `2b9b4e3bf93c76fdfca1249ce0f7ed0bf546aaa0` |
| `components/mybot_platform/boards/m5stack-stick-s3`, `components/mybot_platform/src/drivers/audio/sticks3_es8311_audio.c`, `components/mybot_platform/src/drivers/display/sticks3_st7789_lcd.c` | `github.com/junlon2006/xiaozhi-esp32` | commit `2b9b4e3bf93c76fdfca1249ce0f7ed0bf546aaa0` |
| `components/mybot_platform/boards/zhengchen-1.54tft-wifi/board_config.h` (hardware mapping verification only) | `github.com/junlon2006/xiaozhi-esp32` | commit `2b9b4e3bf93c76fdfca1249ce0f7ed0bf546aaa0` |
| `components/mybot_platform/boards/esp32-s3-touch-amoled-1.75`, `components/mybot_platform/src/drivers/audio/amoled175_codec_audio.c`, `components/mybot_platform/src/drivers/display/amoled175_co5300_lcd.c` | `github.com/junlon2006/xiaozhi-esp32` | commit `2b9b4e3bf93c76fdfca1249ce0f7ed0bf546aaa0` |

Firmware integration differences are limited to the active ESP32-S3 build:

- The AOSL ESP32-S3 HAL uses FreeRTOS delays, byte-sized task stacks, PSRAM allocation, and IPv4
  DSCP/TOS support required by the bundled RTSA package.
- Component Registry manifests and download-cache checksum metadata are omitted because dependencies
  are local or supplied by ESP-IDF v5.5.2.
- The codec-device source set contains only the common adapters and ES8311, ES7243E, ES7210, and
  AW88298 devices used by supported boards; codec-setting errors are propagated to platform callers.
- The TCA95xx constructor removes its I2C device if register reset fails.
- The SenseCAP profile uses explicit IO-expander calls and disables the optional global GPIO API
  wrapper.
- The Wi-Fi component accepts an explicit provisioning SSID and waits for its DNS worker at teardown.
- The button, knob, and SPD2010 components define their pinned version macros without Component
  Registry manifests.
- The M5PM1 production sources retain upstream content with line endings normalized to LF and use
  ESP-IDF's native I2C master API; the optional `i2c_bus` dependency, examples, datasheets, and
  registry metadata are not vendored.
- The CO5300 component contains only its common and SPI/QSPI production paths; MIPI support, tests,
  registry metadata, and caches are omitted. The LCD-touch components likewise omit tests and
  registry metadata.

When updating a dependency, update this file and `THIRD_PARTY_NOTICES.md`, build from a clean
sdkconfig, and report the board validation performed.
