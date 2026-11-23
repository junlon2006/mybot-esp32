# Vendored Dependency Baselines

This file records the revisions of firmware inputs. License and
redistribution terms are documented separately in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

| Path | Source | Pinned revision |
| --- | --- | --- |
| `components/mybot_stack/mybot_sdk/mybot` | `github.com/junlon2006/mybot` | v1.3.0 + post-release updates, commit `8bae576ce12bd18c77b1aeea560ce2f9f9b7f836` |
| `components/mybot_stack/aosl` | `github.com/AgoraIO-Community/aosl` | v1.0.4 + post-release fixes, commit `84e086084ebcd0ae2455a0ce5721950c5fe2e656` |
| `components/mybot_stack/agora_rtc` | Agora RTSA Lite package archive | v1.10.1, build 1270872 (`20260828_194128`) |
| `components/esp-wifi-connect` | `github.com/78/esp-wifi-connect` | 3.2.2, commit `c24b97c194e6b4a1d7be0237b3c28980661cac1e` |
| `components/esp_audio_codec` | ESP Component Registry `espressif/esp_audio_codec` | 2.5.0, commit `3bb83597d07b604e1ab5b78dd4370a28d6fa802d` |
| `components/esp_codec_dev` | `github.com/espressif/esp-adf` | 1.5.11, commit `73befa9ebffdd6e5065b7145329f115910e13ab5` |
| `components/esp_video` | `github.com/espressif/esp-video-components`, `esp_video` | 2.3.0, commit `58d4c6eea08f0f78a2beff27aa4e0155efd74b55` |
| `components/esp_cam_sensor` | `github.com/espressif/esp-video-components`, `esp_cam_sensor` | 2.3.0, commit `58d4c6eea08f0f78a2beff27aa4e0155efd74b55` |
| `components/esp_sccb_intf` | `github.com/espressif/esp-video-components`, `esp_sccb_intf` | 0.0.8, commit `3384d3510c5880edeb0f4c14d9e0196760280637` |
| `components/esp_new_jpeg` | `github.com/espressif/esp-adf-libs`, `esp_new_jpeg` | 0.6.1, commit `35c37e1656db36dbf13b9aee8064d1b59f482f6c`; ESP32-S3 library SHA-256 `4205b1258ce0ef9fd9946abfc7cd5f105b08316ea567e1136cec60b3e6330896` |
| `components/esp_lcd_ili9341` | `github.com/espressif/esp-bsp` | 2.0.2, commit `fc8bd325efcdef6d5802554659debba303058af6` |
| `components/esp_lcd_gc9a01` | ESP LCD GC9A01 production driver | 2.0.4; production source only, local CMake metadata |
| `components/esp_lcd_nv3023` | ESP LCD NV3023 production driver | 1.0.1; production source only, local CMake metadata |
| `components/esp_lcd_sh8601` | ESP LCD SH8601 production driver | 1.0.2; production source only, local CMake metadata |
| `components/lvgl` | `github.com/lvgl/lvgl` | 9.5.0, commit `85aa60d18b3d5e5588d7b247abf90198f07c8a63` |
| `components/esp_lvgl_port` | `github.com/espressif/esp-bsp`, `components/esp_lvgl_port` | 2.8.0~1, commit `d14ff131266bf1392efff88db72cb4638897507c` |
| `components/esp_lcd_spd2010` | ESP Component Registry `espressif/esp_lcd_spd2010` | 2.0.0~1, commit `12f6ca1182ec48889b17ec570fadaaf267cb336e` |
| `components/esp_lcd_co5300` | ESP Component Registry `espressif/esp_lcd_co5300` | 2.1.0, commit `56f3c5620464c061e968d3358bc897c528e097ee`, content hash `21f84c5c825509ebacbddb379555b49092ae165b3e19f0edfbf6f8eff8aa036d` |
| `components/esp_lcd_st77916` | ESP Component Registry `espressif/esp_lcd_st77916` | 2.0.2, commit `91aeb7fb41e8a3e76aeb21371f9f83711c74cf3f` |
| `components/esp_lcd_touch` | ESP Component Registry `espressif/esp_lcd_touch` | 1.2.1, commit `c927778a85eed239dd403c1719d4f543ad56e693`, content hash `3f85a7d95af876f1a6ecca8eb90a81614890d0f03a038390804e5a77e2caf862` |
| `components/esp_lcd_touch_cst816s` | ESP Component Registry `espressif/esp_lcd_touch_cst816s` | 1.1.1~1, commit `ab5696c61e761b272a163740f94b0e5720c3dde4` |
| `components/esp_lcd_touch_cst9217` | ESP Component Registry `waveshare/esp_lcd_touch_cst9217` | 1.0.4, commit `43e6e696bbf0cf8671a97fcfe02c4c720e4e4d27`, content hash `27a00845832b7987cacf4c4125cbed5415d940987583065b01870ac080930856` |
| `components/esp_lcd_touch_ft5x06` | ESP Component Registry `espressif/esp_lcd_touch_ft5x06` | 1.1.0~1; production source and Apache-2.0 license retained |
| `components/esp_lcd_touch_gt911` | ESP Component Registry `espressif/esp_lcd_touch_gt911` | 1.2.0~2; production source and Apache-2.0 license retained |
| `components/esp_io_expander` | ESP Component Registry `espressif/esp_io_expander` | 1.2.1, commit `eb76dc6ecf21ccc4ee7ee58bfea3d3d31fa090cf` |
| `components/esp_io_expander_tca95xx_16bit` | ESP Component Registry `espressif/esp_io_expander_tca95xx_16bit` | 2.0.2, commit `53f6127ba3a1dd80fbdf9a76b759ccd1a8dc0101` |
| `components/button` | ESP Component Registry `espressif/button` | 4.2.0, commit `5f9cb98ae4d0e8153c4b4d1accf471214e5b6fe8` |
| `components/knob` | ESP Component Registry `espressif/knob` | 1.1.0, commit `5f9cb98ae4d0e8153c4b4d1accf471214e5b6fe8` |
| `components/cmake_utilities` | ESP Component Registry `espressif/cmake_utilities` | 0.5.0 |
| `components/m5pm1` | ESP Component Registry `m5stack/m5pm1` | 1.0.7, content hash `731f79d0629e245787440f5419aac5d7a82befeb25f97689d6a2d0331a24a72d` |
| `components/m5ioe1` | ESP Component Registry `m5stack/m5ioe1` | 1.0.9; upstream MIT license retained |
| `components/mybot_platform/src/drivers/audio/es8374_codec.c`, `components/mybot_platform/src/internal/audio/es8374_codec.h`, `components/mybot_platform/src/internal/audio/es_common.h` | Espressif `esp_codec_dev` ES8374 device source | Same 1.5.11 source baseline as `components/esp_codec_dev`; vendored inside the platform component because the local codec-dev source set omits ES8374 |
| Pairing `prompt.ogg` and `0.ogg` through `9.ogg` under `components/mybot_platform/assets/locales/{en-US,zh-CN}` | `github.com/junlon2006/mybot`, `assets/locales` | commit `7854c1d5798004485aa1947ff55116bdb7ab0237`; 16 kHz mono s16 PCM converted to 20 ms Opus/Ogg; source/output SHA-256 and encoding command in `components/mybot_platform/assets/locales/SOURCES.json` |
| `components/mybot_platform/src/drivers/display/renderers/lvgl/lvgl_fonts.c`, `components/mybot_platform/src/drivers/display/assets/licenses/LVGL_VIEW_FONT_LICENSE.txt` | `github.com/lvgl/lvgl`, `scripts/built_in_font/SourceHanSansSC-Normal.otf` | LVGL 9.5.0 source font SHA-256 `1ee89e1669362dee13851129c0a8a791a87521eb4148e5efbf5d26596738e25b`; generated 20 px, 4 bpp static UI subset |
| `components/mybot_platform/assets/ui/noto_emoji`, `components/mybot_platform/src/drivers/display/renderers/lvgl/lvgl_assets.c` | Noto Color Emoji font glyphs rasterized by `github.com/78/noto-fonts`, `png/noto-color-emoji_64` | 2.0.0, commit `d45dbc64052d57048f20ab1770074172ce9eb53b`; per-image SHA-256 values and glyph provenance in `assets/ui/noto_emoji/SOURCES.json` |

## Board profiles

Board documentation describes the checked-in ESP32-S3 profiles. Build settings, pins and driver
selection are defined by `board.cmake`, `board_config.h` and `sdkconfig.defaults` in each profile.
See the [board documentation index](components/mybot_platform/boards/README.md) for all supported
boards and the project build/flash workflow.

## Implementation references

External implementation references and hardware-mapping verification baselines used for
project-maintained board ports are pinned separately; their application layer and dependency set
are not vendored into this repository.

| Paths | Source | Pinned revision |
| --- | --- | --- |
| CoreS3 `components/mybot_platform/src/drivers/display/renderers/lvgl/pixel_glasses_view.cc`, expression geometry/masks/generators, `pixel_expressions.c`, and private expression headers/policy | `github.com/junlon2006/bk_solution_ai`, Solution checkout in `mybot-bk7259` | commit `dc4c85492bcf00f780c92d56c6cba2a362f54f68`; adapted to the existing ESP32 display API and CoreS3 320x240 geometry |
| CoreS3 terminal subsets in `components/mybot_platform/src/drivers/display/renderers/lvgl/pixel_fonts.c` and `TERMINAL_FONT_LICENSE.txt` | DejaVu Sans Mono Bold, renamed MyBot Terminal | Source TTF SHA-256 `3a3c502eeff669a231549e80df9f7c49de109bafe303170409e905d0b31a38fe`; 9 px terminal and 10 px signature subsets, with retained Bitstream Vera terms |
| `components/mybot_platform/src/drivers/display/adapters/lvgl/shared_lvgl_adapter.cc`, `components/mybot_platform/src/internal/display/display_panel.h` | Project-maintained shared adapter for ST7789, CO5300, SPD2010, and ST77916 panels | Uses the board panel drivers; no new upstream source |
| `components/mybot_platform/src/drivers/display/panels/generic/rgb565_spi_panel.c` GC9D01N, GC9309NA and ST7796 initialization profiles; `generic/i80_st7789_panel.c` | Board hardware initialization values and ESP-IDF i80 API | Only controller register values are adapted; the ESP LCD command driver, platform lifecycle and cleanup remain project-maintained or use the pinned Apache-2.0 `esp_lcd_gc9a01` component |
| `components/mybot_platform/boards/respeaker-flex-xvf3800-circular4-xiao`, `components/mybot_platform/src/drivers/audio/xvf3800_audio.c`, `partitions/v2/8m.csv` | `github.com/qiuyanli1990/respeaker-flex-circle-Agora-mybot` | commit `b06024382eb104c998aead4841e1df647193065b` |
| `components/mybot_platform/boards/bread-compact-wifi-lcd/bread_input.c` | Existing local `components/mybot_platform/boards/atom-echos3r/atom_echos3r_input.c` input implementation | Project-local adaptation for Boot GPIO0 |
| `components/mybot_platform/boards/esp-vocat/board_config.h` (hardware mapping verification only) | `github.com/espressif/esp-brookesia` | commit `b22c488f50bafe53342c8e171081bd736396ef58` |

Firmware integration differences are limited to the active ESP32-S3 build:

- Only `m5stack-core-s3` selects the process-gray pixel-glasses view. CMake chooses it from
  `MYBOT_BOARD`, with no manual renderer flag; the other profiles retain their current
  UI/hardware. The original Source Han Sans 20 px UI subset is reused, while the CoreS3 path
  uses fixed widgets/curves, five immutable A8 masks (13,631 alpha bytes), and the terminal
  subsets above instead of the shared view's four emoji. Audio, camera/video policy, input,
  power, PSRAM, partitions, SDK, AOSL and RTSA inputs are unchanged.
  Geometry and colors refer to the user-provided threejs simulator. The reference hashes are
  `1d51ca444e69a39da5b2feedc38d543aea362433184e89a76a825e757b684e31` (`expressions.js`) and
  `2b0cf05ffceb91f079d1cd542a982622e9c44c882ff84ac98a84b9a90bfa0646` (`reference-style.js`).
  No independent simulator license declaration was found; MIT/Apache-2.0 notices apply to the
  reused BK display sources/generators as stated in their files, not to an invented simulator license.
- The Xingzhi Cube port reuses project audio/input/LVGL drivers and introduces no new vendored
  dependency. Its Board lifecycle and pin mapping are defined by the local profile.
  The 16 MB Flash and 80 MHz Octal PSRAM profile requires real-device capacity confirmation.
- The Bread Compact Wi-Fi LCD port fixes the user-selected N16R8, 240x320 ST7789, and separate
  I2S wiring. It reuses raw audio, the existing ST7789 panel, and shared LVGL; its independent
  Boot-only input is adapted from the local AtomEchoS3R input implementation. Board lifecycle and
  build metadata are project-maintained Apache-2.0. Chinese and English build support has been
  validated; real-device validation remains pending.
- The AMOLED 2.16 profile owns independent Board and CO5300 480-pixel panel variants derived
  from the existing integration. It reuses AXP2101 configuration, codecs, CST9217, and shared LVGL
  with independent identity and 480x480 zero-gap geometry.
  Its conservative 16 MB Flash/Octal PSRAM defaults require physical capacity confirmation;
  no new dependency or upstream application service is added.
- The GenjuTech 1.54, AMOLED 1.8 V2 and LCD 1.85 profiles add independent Board identities,
  TCA9554 state handling, CST816S/Boot input, CO5300 368x448 and ST77916 360x360 panel variants,
  and no new package dependency. The LCD 1.85 raw-I2S path exposes 32-bit right-slot capture and
  duplicates mono playback into both slots while the SDK boundary remains 16 kHz mono signed-16
  PCM. Build success and any recorded image do not establish panel, audio, touch, power or radio
  behavior on hardware.
- LVGL and its ESP-IDF port are local, pinned dependencies included automatically for display
  boards and omitted for headless boards. The build omits examples, tests, and registry download metadata. The UI uses
  a static bilingual font subset and existing public LCD state; no upstream application services,
  asset download protocol, or SDK internals are imported.
  `scripts/generate-lvgl-font.py` generates the font using Pillow 10.2.0 and FreeType 2.13.2;
  the checked-in glyph data is sufficient for firmware builds without the generator or source OTF.
  LVGL's local allocator keeps its heap in PSRAM; local component configuration selects the
  required rendering features and fonts without changing its upstream source files. Integration
  details are in `components/lvgl/README.integration.md`. The ESP-IDF port's runtime, display
  rollback, and deinit contract are adapted for repeated initialization and teardown; the exact
  local corrections are recorded in `components/esp_lvgl_port/PATCHES.md`.
  The shared workflow view on other display boards enables the unchanged upstream LVGL image widget. Four pinned local emoji PNGs
  are converted by `scripts/generate-lvgl-ui-assets.py` with Pillow 10.2.0 into 64x64 straight-alpha
  ARGB8888 constants (BGRA byte order), totaling 65,536 pixel bytes in Flash. The script pads the
  64x61 source images without scaling; the firmware does not include PNG/GIF decoding or an image
  cache. Original PNGs and their font license are retained for reproducibility.

- The video components include only the GC0308 DVP path, native I2C SCCB, and the ESP32-S3 JPEG
  library. Local CMake/Kconfig replace registry metadata and pin component versions; sensor
  detection uses the static table, the default format is QVGA YUYV, and SCCB timeout is 100 ms.
  With video enabled, `main/Kconfig.projbuild` overrides the component's 16,384-byte DVP DMA-size
  default with 8,192 bytes; the vendored Kconfig and driver remain unchanged. The driver's frame
  alignment allocates 7,680 bytes (7.5 KiB) of contiguous internal DMA memory for QVGA YUYV.
  The platform supplies capture buffers
  at most five times per second while retaining the sensor's 20 fps register configuration.
  Unused transports, sensors, examples, and registry caches are omitted. Video sources and the
  JPEG binary are linked only with `CONFIG_MYBOT_ENABLE_VIDEO=y`.

- The AOSL ESP32-S3 HAL uses FreeRTOS delays, byte-sized task stacks, PSRAM allocation, and IPv4
  DSCP/TOS support required by the bundled RTSA package.
- `esp-wifi-connect` declares the MIT-licensed `espressif/cjson` 1.7.19~2 managed dependency
  because ESP-IDF 6.1 no longer supplies cJSON as a built-in component.
- Component Registry manifests and download-cache checksum metadata are omitted because dependencies
  are local or supplied by ESP-IDF v6.1.
- The codec-device source set contains only the common adapters and ES8311, ES8388, ES7243E,
  ES7210, and AW88298 devices used by supported boards; codec-setting errors are propagated to
  platform callers.
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
