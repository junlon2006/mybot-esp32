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
| `components/button` | ESP Component Registry `espressif/button` | 4.2.0, commit `5f9cb98ae4d0e8153c4b4d1accf471214e5b6fe8` |
| `components/cmake_utilities` | ESP Component Registry `espressif/cmake_utilities` | 0.5.0 |
| `components/mybot_platform/assets` | `github.com/junlon2006/mybot-bk7258` | commit `2577b5977a9f137855a7acf1fcdcd4040c5db2ea` |

External implementation references used for project-maintained board ports are pinned separately;
their application layer and dependency set are not vendored into this repository.

| Paths | Source | Pinned revision |
| --- | --- | --- |
| `components/mybot_platform/boards/respeaker-flex-xvf3800-circular4-xiao`, `components/mybot_platform/src/drivers/audio/xvf3800_audio.c`, `partitions/v2/8m.csv` | `github.com/qiuyanli1990/respeaker-flex-circle-Agora-mybot` | commit `b06024382eb104c998aead4841e1df647193065b` |

Firmware integration differences are limited to the active ESP32-S3 build:

- The AOSL ESP32-S3 HAL uses FreeRTOS delays, byte-sized task stacks, PSRAM allocation, and IPv4
  DSCP/TOS support required by the bundled RTSA package.
- Component Registry manifests and download-cache checksum metadata are omitted because dependencies
  are local or supplied by ESP-IDF v5.5.2.
- The codec-device source set contains only the common adapters and devices used by supported boards.
- The Wi-Fi component accepts an explicit provisioning SSID and waits for its DNS worker at teardown.
- The button component defines its pinned version macros without the Component Registry manifest.

When updating a dependency, update this file and `THIRD_PARTY_NOTICES.md`, build from a clean
sdkconfig, and report the board validation performed.
