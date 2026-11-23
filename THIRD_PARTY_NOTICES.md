# Third-Party Notices

The root Apache-2.0 license applies only to project-maintained code unless a file states otherwise.
It does not replace third-party terms. This file is informational and is not legal advice.

| Component | Terms | Location |
| --- | --- | --- |
| mybot SDK 1.3.0 + post-release updates (`8bae576`) | Apache-2.0; bundled JSON parser portions MIT | `components/mybot_stack/mybot_sdk/mybot` |
| AOSL (`84e0860`) | Bundled license: Apache-2.0 text plus additional deployment restrictions | `components/mybot_stack/aosl` |
| Agora RTSA Lite for ESP32-S3 1.10.1 (build 1270872) | Separate Agora distribution terms | `components/mybot_stack/agora_rtc` |
| esp-wifi-connect 3.2.2 | MIT | `components/esp-wifi-connect` |
| cJSON 1.7.19~2 | MIT | Managed dependency `espressif/cjson`, declared by `components/esp-wifi-connect/idf_component.yml` |
| Espressif audio codec 2.5.0 | Espressif Modified MIT and file-specific terms | `components/esp_audio_codec` |
| Espressif codec device 1.5.11 | Apache-2.0 | `components/esp_codec_dev` |
| Espressif video 2.3.0 (DVP subset) | Espressif MIT; Linux UAPI headers retain file-specific BSD/GPL-with-syscall-exception terms | `components/esp_video` |
| Espressif camera sensor 2.3.0 (GC0308/DVP subset) | Apache-2.0 | `components/esp_cam_sensor` |
| Espressif SCCB 0.0.8 (I2C subset) | Apache-2.0 | `components/esp_sccb_intf` |
| Espressif JPEG 0.6.1 (ESP32-S3 binary) | Espressif MIT, use on Espressif products | `components/esp_new_jpeg` |
| Espressif ILI9341 LCD driver 2.0.2 | Apache-2.0 | `components/esp_lcd_ili9341` |
| Espressif GC9A01 LCD driver 2.0.4 | Apache-2.0 | `components/esp_lcd_gc9a01` |
| Espressif NV3023 LCD driver 1.0.1 | Apache-2.0 | `components/esp_lcd_nv3023` |
| Espressif SH8601 LCD driver 1.0.2 | Apache-2.0 | `components/esp_lcd_sh8601` |
| LVGL 9.5.0 | MIT plus retained font/library-specific terms | `components/lvgl` |
| Espressif LVGL port 2.8.0~1 | Apache-2.0 | `components/esp_lvgl_port` |
| Espressif SPD2010 LCD driver 2.0.0~1 | Apache-2.0 | `components/esp_lcd_spd2010` |
| Espressif CO5300 LCD driver 2.1.0 | Apache-2.0 | `components/esp_lcd_co5300` |
| Espressif ST77916 LCD driver 2.0.2 | Apache-2.0 | `components/esp_lcd_st77916` |
| Espressif LCD touch 1.2.1 | Apache-2.0 | `components/esp_lcd_touch` |
| Espressif CST816S touch driver 1.1.1~1 | Apache-2.0 | `components/esp_lcd_touch_cst816s` |
| Waveshare CST9217 touch driver 1.0.4 | Apache-2.0 | `components/esp_lcd_touch_cst9217` |
| Espressif FT5x06 touch driver 1.1.0~1 | Apache-2.0 | `components/esp_lcd_touch_ft5x06` |
| Espressif GT911 touch driver 1.2.0~2 | Apache-2.0 | `components/esp_lcd_touch_gt911` |
| Espressif IO expander 1.2.1 | Apache-2.0 | `components/esp_io_expander` |
| Espressif TCA95xx 16-bit IO expander 2.0.2 | Apache-2.0 | `components/esp_io_expander_tca95xx_16bit` |
| Espressif button 4.2.0 | Apache-2.0 | `components/button` |
| Espressif knob 1.1.0 | Apache-2.0 | `components/knob` |
| Espressif CMake utilities 0.5.0 | Apache-2.0 | `components/cmake_utilities` |
| M5Stack M5PM1 1.0.7 | MIT | `components/m5pm1` |
| M5Stack M5IOE1 1.0.9 | MIT | `components/m5ioe1` |
| Espressif ES8374 codec source | Apache-2.0 | `components/mybot_platform/src/drivers/audio/es8374_codec.c` and its private headers |
| Pairing prompt and digit audio (`7854c1d` SDK reference) | Apache-2.0 | `prompt.ogg` and `0.ogg` through `9.ogg` in `components/mybot_platform/assets/locales/{en-US,zh-CN}` |
| Source Han Sans UI glyph subset | SIL Open Font License 1.1 | `components/mybot_platform/src/drivers/display/renderers/lvgl/lvgl_fonts.c`, `components/mybot_platform/src/drivers/display/assets/licenses/LVGL_VIEW_FONT_LICENSE.txt` |
| Noto Color Emoji rasterized glyphs | SIL Open Font License 1.1 | `components/mybot_platform/assets/ui/noto_emoji`, `components/mybot_platform/src/drivers/display/renderers/lvgl/lvgl_assets.c` |
| CoreS3 pixel-glasses renderer, geometry/mask generators (`dc4c854` BK reference) | MIT; expression helpers/policy retain their file-level Apache-2.0 notices | `components/mybot_platform/src/drivers/display/renderers/lvgl/pixel_glasses_view.cc`, pixel geometry/masks/generators and private expression support |
| MyBot Terminal bitmap subsets derived from DejaVu Sans Mono Bold | Bitstream Vera font terms, with public-domain DejaVu changes | `components/mybot_platform/src/drivers/display/renderers/lvgl/pixel_fonts.c`, `TERMINAL_FONT_LICENSE.txt` |
| Project-maintained shared LVGL panel adapters | Apache-2.0 unless file SPDX says otherwise | `components/mybot_platform/src/drivers/display/adapters/lvgl/st7789_lvgl_adapter.cc`, `components/mybot_platform/src/drivers/display/adapters/lvgl/shared_lvgl_adapter.cc` |
| ESP-IDF | Apache-2.0 plus component-specific terms | External development SDK |

The built-in LVGL UI fonts retain their separate Montserrat OFL and Font Awesome terms at
`components/lvgl/scripts/built_in_font/font_license/Montserrat/OFL.txt` and
`components/lvgl/scripts/built_in_font/font_license/FontAwesome5/LICENSE.txt`.

The shared workflow view's four local UI emoji are rasterized Noto Color Emoji font glyphs, supplied by the pinned
`78/noto-fonts` package and converted into constant pixels for this firmware. They retain
Google's copyright and the font's SIL OFL 1.1 terms in
`components/mybot_platform/assets/ui/noto_emoji/LICENSE.txt`. Source PNG hashes, Unicode glyphs,
and the conversion method are recorded in that directory's `SOURCES.json`.

## AOSL

The bundled AOSL license is based on Apache-2.0 and adds material deployment restrictions. Read
`components/mybot_stack/aosl/LICENSE` before using, modifying, deploying, or redistributing firmware containing
AOSL. Do not describe the combined repository or firmware image as uniformly Apache-2.0.

## Agora RTSA

The repository contains an ESP32-S3 static RTSA library. No standalone license or NOTICE was
included in the supplied package. Possession of the binary is not evidence of redistribution
rights. Obtain and retain the applicable Agora license and written redistribution authorization
before publishing source archives, firmware images, mirrors, or releases containing this library.
If authorization is unavailable, exclude the library from public artifacts and require users to
supply an authorized package locally.

## Component Notices

The pinned esp-wifi-connect manifest declares MIT but its 3.2.2 archive omitted the license file.
`components/esp-wifi-connect/LICENSE` is the byte-identical MIT text later published by that
upstream project in commit `347682fa013b52f863052ad4b1a793ca3cabff17`.

The vendored M5PM1 production sources retain the upstream M5Stack MIT license in
`components/m5pm1/LICENSE`.

The CoreS3 pixel-glasses renderer and geometry generators use the file-level terms in their
sources. The terminal font subsets retain the Bitstream Vera terms and public-domain DejaVu
changes in `components/mybot_platform/src/drivers/display/renderers/lvgl/TERMINAL_FONT_LICENSE.txt`.
Source Han Sans UI glyphs retain the SIL OFL 1.1 terms in
`components/mybot_platform/src/drivers/display/assets/licenses/LVGL_VIEW_FONT_LICENSE.txt`.

## Vendored SDK Notice

The Chinese and English pairing prompt and digit recordings are derived from the SDK's
`assets/locales` PCM files at commit `7854c1d5798004485aa1947ff55116bdb7ab0237`, under Apache-2.0.
Their encoded Ogg files retain the SDK license through `components/mybot_stack/mybot_sdk/mybot/LICENSE`;
source/output checksums and encoding settings are recorded in
`components/mybot_platform/assets/locales/SOURCES.json`.

The original mybot SDK license and third-party notice are retained unchanged under
`components/mybot_stack/mybot_sdk/mybot/`. That notice may describe packages used by other mybot targets; it is preserved
as part of the immutable SDK snapshot and does not redefine this firmware's dependency set.

Every component license file and file-level SPDX/copyright notice remains in force.
