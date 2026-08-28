# Vendored source baselines

本工程将构建所需的依赖源码固定在仓库中，避免 ESP-IDF Component Registry 版本漂移。

| 路径 | 来源 | 固定版本 |
| --- | --- | --- |
| `third_party/mybot` | `junlon2006/mybot` | v1.0.0 + Unreleased, commit `27324e7177b52ad9d8743aba31acf94d0a125f44` |
| `components/aosl` | `AgoraIO-Community/aosl` | v1.0.4 + post-release fixes, commit `84e086084ebcd0ae2455a0ce5721950c5fe2e656` |
| `components/agora_rtc` | `Agora RTSA Lite package` | v1.10.1, build 1270872 (`20260828_194128`) |
| `components/esp-wifi-connect` | `78/esp-wifi-connect` | 3.2.2, commit `c24b97c194e6b4a1d7be0237b3c28980661cac1e` |
| `components/esp_audio_codec` | Espressif `esp_audio_codec` | 2.5.0, commit `3bb83597d07b604e1ab5b78dd4370a28d6fa802d` |
| `components/esp_codec_dev` | Espressif `esp_codec_dev` | 1.5.11, esp-adf commit `73befa9ebffdd6e5065b7145329f115910e13ab5` |
| `components/esp_lcd_ili9341` | Espressif `esp_lcd_ili9341` | 2.0.2, esp-bsp commit `fc8bd325efcdef6d5802554659debba303058af6` |
| `components/button` | `espressif/button` | 4.2.0, commit `5f9cb98ae4d0e8153c4b4d1accf471214e5b6fe8` |
| `components/cmake_utilities` | Espressif cmake utilities | 0.5.0 |
| `components/mybot_platform/assets` | `junlon2006/mybot-bk7258` prompt assets | commit `2577b5977a9f137855a7acf1fcdcd4040c5db2ea` |
| `components/mybot_platform/boards/m5stack-core-s3` | `junlon2006/xiaozhi-esp32` CoreS3 reference | commit `ad2f7da4c9ba77294a5abb48f29e895fe486ed0e` |

Local changes relative to those baselines:

- `components/aosl/platform/src/esp32-s3/aosl_hal_thread.c` passes AOSL's byte-sized stack budget
  directly to ESP-IDF `xTaskCreate()`, whose stack parameter is also measured in bytes.
- `components/aosl/platform/src/esp32-s3/aosl_hal_time.c` uses `vTaskDelay()` so millisecond waits
  yield to FreeRTOS and do not starve the idle task.
- `components/aosl/platform/src/esp32-s3/aosl_hal_memory.c` allocates from PSRAM when available,
  with the ESP32-S3 platform's 8-bit heap capability.
- The latest AOSL socket DSCP API (`aosl_set_socket_dscp` and the ESP32-S3 IPv4 TOS HAL path) is
  mapped into the ESP32-S3 platform directory for compatibility with RTSA 1.10.1.
- The component retains the ESP32-S3 platform directory and only the platform adapters used by this
  firmware; unsupported upstream platform ports are not part of the firmware component.
- Vendored component manifests are omitted. Dependencies are local or supplied by ESP-IDF v5.5.2.
- The esp_codec_dev source set is limited to its common interfaces and platform adapters plus the
  ES7210 ADC and AW88298 DAC used by M5Stack CoreS3.
- The M5Stack CoreS3 pin map, AXP2101/AW9523 sequencing, ILI9342 setup, FT6336 polling, and
  ES7210/AW88298 bus configuration follow the xiaozhi baseline commit recorded above.
- The esp-wifi-connect wrapper accepts an explicit provisioning SSID so the board port can use its
  product-specific short name without the component appending another MAC suffix.
- The esp-wifi-connect DNS server validates request sizes and joins its worker before releasing
  provisioning resources.
- The button wrapper defines its pinned `4.2.0` version macros directly because the upstream
  package helper normally reads those values from the omitted Registry manifest.

When updating a dependency, update this file, `THIRD_PARTY_NOTICES.md`, build with a clean sdkconfig,
and report the target-board validation performed.
