# Vendored Dependency Baselines

This file records only the revisions needed to reproduce the firmware build. License and
redistribution terms are documented separately in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

| Path | Source | Pinned revision |
| --- | --- | --- |
| `third_party/mybot` | `github.com/junlon2006/mybot` | v1.2.0 + post-release fixes, commit `5b7a6c1537f238580a0a523840274c9911a8efcf` |
| `components/aosl` | `github.com/AgoraIO-Community/aosl` | v1.0.4 + post-release fixes, commit `84e086084ebcd0ae2455a0ce5721950c5fe2e656` |
| `components/agora_rtc` | Agora RTSA Lite package archive | v1.10.1, build 1270872 (`20260828_194128`) |
| `components/esp-wifi-connect` | `github.com/78/esp-wifi-connect` | 3.2.2, commit `c24b97c194e6b4a1d7be0237b3c28980661cac1e` |
| `components/esp_audio_codec` | ESP Component Registry `espressif/esp_audio_codec` | 2.5.0, commit `3bb83597d07b604e1ab5b78dd4370a28d6fa802d` |
| `components/esp_codec_dev` | `github.com/espressif/esp-adf` | 1.5.11, commit `73befa9ebffdd6e5065b7145329f115910e13ab5` |
| `components/esp_video` | `github.com/espressif/esp-video-components`, `esp_video` | 2.3.0, commit `58d4c6eea08f0f78a2beff27aa4e0155efd74b55` |
| `components/esp_cam_sensor` | `github.com/espressif/esp-video-components`, `esp_cam_sensor` | 2.3.0, commit `58d4c6eea08f0f78a2beff27aa4e0155efd74b55` |
| `components/esp_sccb_intf` | `github.com/espressif/esp-video-components`, `esp_sccb_intf` | 0.0.8, commit `3384d3510c5880edeb0f4c14d9e0196760280637` |
| `components/esp_new_jpeg` | `github.com/espressif/esp-adf-libs`, `esp_new_jpeg` | 0.6.1, commit `35c37e1656db36dbf13b9aee8064d1b59f482f6c`; ESP32-S3 library SHA-256 `4205b1258ce0ef9fd9946abfc7cd5f105b08316ea567e1136cec60b3e6330896` |
| `components/esp_lcd_ili9341` | `github.com/espressif/esp-bsp` | 2.0.2, commit `fc8bd325efcdef6d5802554659debba303058af6` |
| `components/lvgl` | `github.com/lvgl/lvgl` | 9.5.0, commit `85aa60d18b3d5e5588d7b247abf90198f07c8a63` |
| `components/esp_lvgl_port` | `github.com/espressif/esp-bsp`, `components/esp_lvgl_port` | 2.8.0~1, commit `d14ff131266bf1392efff88db72cb4638897507c` |
| `components/esp_lcd_spd2010` | ESP Component Registry `espressif/esp_lcd_spd2010` | 2.0.0~1, commit `12f6ca1182ec48889b17ec570fadaaf267cb336e` |
| `components/esp_lcd_co5300` | ESP Component Registry `espressif/esp_lcd_co5300` | 2.1.0, commit `56f3c5620464c061e968d3358bc897c528e097ee`, content hash `21f84c5c825509ebacbddb379555b49092ae165b3e19f0edfbf6f8eff8aa036d` |
| `components/esp_lcd_st77916` | ESP Component Registry `espressif/esp_lcd_st77916` | 2.0.2, commit `91aeb7fb41e8a3e76aeb21371f9f83711c74cf3f` |
| `components/esp_lcd_touch` | ESP Component Registry `espressif/esp_lcd_touch` | 1.2.1, commit `c927778a85eed239dd403c1719d4f543ad56e693`, content hash `3f85a7d95af876f1a6ecca8eb90a81614890d0f03a038390804e5a77e2caf862` |
| `components/esp_lcd_touch_cst816s` | ESP Component Registry `espressif/esp_lcd_touch_cst816s` | 1.1.1~1, commit `ab5696c61e761b272a163740f94b0e5720c3dde4` |
| `components/esp_lcd_touch_cst9217` | ESP Component Registry `waveshare/esp_lcd_touch_cst9217` | 1.0.4, commit `43e6e696bbf0cf8671a97fcfe02c4c720e4e4d27`, content hash `27a00845832b7987cacf4c4125cbed5415d940987583065b01870ac080930856` |
| `components/esp_io_expander` | ESP Component Registry `espressif/esp_io_expander` | 1.2.1, commit `eb76dc6ecf21ccc4ee7ee58bfea3d3d31fa090cf` |
| `components/esp_io_expander_tca95xx_16bit` | ESP Component Registry `espressif/esp_io_expander_tca95xx_16bit` | 2.0.2, commit `53f6127ba3a1dd80fbdf9a76b759ccd1a8dc0101` |
| `components/button` | ESP Component Registry `espressif/button` | 4.2.0, commit `5f9cb98ae4d0e8153c4b4d1accf471214e5b6fe8` |
| `components/knob` | ESP Component Registry `espressif/knob` | 1.1.0, commit `5f9cb98ae4d0e8153c4b4d1accf471214e5b6fe8` |
| `components/cmake_utilities` | ESP Component Registry `espressif/cmake_utilities` | 0.5.0 |
| `components/m5pm1` | ESP Component Registry `m5stack/m5pm1` | 1.0.7, content hash `731f79d0629e245787440f5419aac5d7a82befeb25f97689d6a2d0331a24a72d` |
| `components/mybot_platform/assets/locales` | `github.com/junlon2006/mybot-bk7258` | commit `2577b5977a9f137855a7acf1fcdcd4040c5db2ea` |
| `components/mybot_platform/src/drivers/display/ili9342_lcd_font.inc`, `components/mybot_platform/src/drivers/display/OFL-1.1.txt` | `github.com/junlon2006/mybot-bk7259` vendored SDK fonts | OFL-1.1 Liberation Sans glyphs |
| `components/mybot_platform/src/drivers/display/cores3_lvgl_font.c`, `components/mybot_platform/src/drivers/display/CORES3_FONT_LICENSE.txt` | `github.com/lvgl/lvgl`, `scripts/built_in_font/SourceHanSansSC-Normal.otf` | LVGL 9.5.0 source font SHA-256 `1ee89e1669362dee13851129c0a8a791a87521eb4148e5efbf5d26596738e25b`; generated 20 px, 4 bpp static UI subset |
| `components/mybot_platform/assets/ui/noto_emoji`, `components/mybot_platform/src/drivers/display/cores3_ui_assets.c` | Noto Color Emoji font glyphs rasterized by `github.com/78/noto-fonts`, `png/noto-color-emoji_64` | 2.0.0, commit `d45dbc64052d57048f20ab1770074172ce9eb53b`; per-image SHA-256 values and glyph provenance in `assets/ui/noto_emoji/SOURCES.json` |

External implementation references and hardware-mapping verification baselines used for
project-maintained board ports are pinned separately; their application layer and dependency set
are not vendored into this repository.

| Paths | Source | Pinned revision |
| --- | --- | --- |
| `components/mybot_platform/src/drivers/display/cores3_lvgl_view.cc`, `components/mybot_platform/src/internal/cores3_lvgl_view.h` | `github.com/junlon2006/xiaozhi-esp32`, `main/display/lcd_display.cc` and LVGL theme layout | commit `1d5eeb2dd51cb315f98ef3c7d3f2b96bd2bbcf1d` |
| `components/mybot_platform/boards/respeaker-flex-xvf3800-circular4-xiao`, `components/mybot_platform/src/drivers/audio/xvf3800_audio.c`, `partitions/v2/8m.csv` | `github.com/qiuyanli1990/respeaker-flex-circle-Agora-mybot` | commit `b06024382eb104c998aead4841e1df647193065b` |
| `components/mybot_platform/boards/sensecap-watcher`, `components/mybot_platform/src/drivers/audio/sensecap_codec_audio.c`, `components/mybot_platform/src/drivers/display/spd2010_lcd.c`, `partitions/v2/32m-sensecap.csv` | `github.com/junlon2006/xiaozhi-esp32` | commit `2b9b4e3bf93c76fdfca1249ce0f7ed0bf546aaa0` |
| `components/mybot_platform/boards/m5stack-stick-s3`, `components/mybot_platform/src/drivers/audio/sticks3_es8311_audio.c`, `components/mybot_platform/src/drivers/display/sticks3_st7789_lcd.c` | `github.com/junlon2006/xiaozhi-esp32` | commit `2b9b4e3bf93c76fdfca1249ce0f7ed0bf546aaa0` |
| `components/mybot_platform/boards/zhengchen-1.54tft-wifi/board_config.h` (hardware mapping verification only) | `github.com/junlon2006/xiaozhi-esp32` | commit `2b9b4e3bf93c76fdfca1249ce0f7ed0bf546aaa0` |
| `components/mybot_platform/boards/esp32-s3-touch-amoled-1.75-common`, `components/mybot_platform/boards/esp32-s3-touch-amoled-1.75`, `components/mybot_platform/boards/esp32-s3-touch-amoled-1.75c`, `components/mybot_platform/src/drivers/audio/amoled175_codec_audio.c`, `components/mybot_platform/src/drivers/display/amoled175_co5300_lcd.c` | `github.com/junlon2006/xiaozhi-esp32` | commit `2b9b4e3bf93c76fdfca1249ce0f7ed0bf546aaa0` |
| `components/mybot_platform/boards/esp-vocat`, `components/mybot_platform/src/drivers/audio/vocat_codec_audio.c`, `components/mybot_platform/src/drivers/display/vocat_st77916_lcd.c` | `github.com/junlon2006/xiaozhi-esp32` | commit `2b9b4e3bf93c76fdfca1249ce0f7ed0bf546aaa0` |
| `components/mybot_platform/boards/esp-vocat/board_config.h` (hardware mapping verification only) | `github.com/espressif/esp-brookesia` | commit `b22c488f50bafe53342c8e171081bd736396ef58` |

Firmware integration differences are limited to the active ESP32-S3 build:

- LVGL and its ESP-IDF port are local, pinned dependencies used only by the optional CoreS3 LVGL
  backend. The build omits examples, tests, and registry download metadata. The UI uses a static
  bilingual font subset and existing public LCD state; no upstream application services, asset
  download protocol, or SDK internals are imported.
  `scripts/generate-cores3-lvgl-font.py` generates the font using Pillow 10.2.0 and FreeType 2.13.2;
  the checked-in glyph data is sufficient for firmware builds without the generator or source OTF.
  LVGL's local allocator keeps its heap in PSRAM; local component configuration selects the
  required rendering features and fonts without changing its upstream source files. Integration
  details are in `components/lvgl/README.integration.md`. The ESP-IDF port's runtime, display
  rollback, and deinit contract are adapted for repeated initialization and teardown; the exact
  local corrections are recorded in `components/esp_lvgl_port/PATCHES.md`.
  The UI enables the unchanged upstream LVGL image widget. Four pinned local emoji PNGs
  are converted by `scripts/generate-cores3-ui-assets.py` with Pillow 10.2.0 into 64x64 straight-alpha
  ARGB8888 constants (BGRA byte order), totaling 65,536 pixel bytes in Flash. The script pads the
  64x61 source images without scaling; the firmware does not include PNG/GIF decoding or an image
  cache. Original PNGs and their font license are retained for reproducibility.

- The video components include only the GC0308 DVP path, native I2C SCCB, and the ESP32-S3 JPEG
  library. Local CMake/Kconfig replace registry metadata and pin component versions; sensor
  detection uses the static table, the default format is QVGA YUYV, and SCCB timeout is 100 ms.
  Unused transports, sensors, examples, and registry caches are omitted. Video sources and the
  JPEG binary are linked only with `CONFIG_MYBOT_ENABLE_VIDEO=y`.

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
- The CO5300 and ST77916 components contain only the production paths required by supported boards;
  unused interfaces, tests, registry metadata, and caches are omitted. The LCD-touch components
  likewise omit tests and registry metadata.
- The ST77916 and CST816S production sources have formatting normalized for this repository. The
  CST816S unused ID helper follows its Kconfig guard so the ESP-VoCat no-ID build remains warning
  free.

When updating a dependency, update this file and `THIRD_PARTY_NOTICES.md`, build from a clean
sdkconfig, and report the board validation performed.
