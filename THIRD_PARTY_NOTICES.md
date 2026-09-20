# Third-Party Notices

The root Apache-2.0 license applies only to project-maintained code unless a file states otherwise.
It does not replace third-party terms. This file is informational and is not legal advice.

| Component | Terms | Location |
| --- | --- | --- |
| mybot SDK 1.2.0 + post-release fixes (`5b7a6c1`) | Apache-2.0; bundled JSON parser portions MIT | `third_party/mybot` |
| AOSL (`84e0860`) | Bundled license: Apache-2.0 text plus additional deployment restrictions | `components/aosl` |
| Agora RTSA Lite for ESP32-S3 1.10.1 (build 1270872) | Separate Agora distribution terms | `components/agora_rtc` |
| esp-wifi-connect 3.2.2 | MIT | `components/esp-wifi-connect` |
| Espressif audio codec 2.5.0 | Espressif Modified MIT and file-specific terms | `components/esp_audio_codec` |
| Espressif codec device 1.5.11 | Apache-2.0 | `components/esp_codec_dev` |
| Espressif video 2.3.0 (DVP subset) | Espressif MIT; Linux UAPI headers retain file-specific BSD/GPL-with-syscall-exception terms | `components/esp_video` |
| Espressif camera sensor 2.3.0 (GC0308/DVP subset) | Apache-2.0 | `components/esp_cam_sensor` |
| Espressif SCCB 0.0.8 (I2C subset) | Apache-2.0 | `components/esp_sccb_intf` |
| Espressif JPEG 0.6.1 (ESP32-S3 binary) | Espressif MIT, use on Espressif products | `components/esp_new_jpeg` |
| Espressif ILI9341 LCD driver 2.0.2 | Apache-2.0 | `components/esp_lcd_ili9341` |
| LVGL 9.5.0 | MIT plus retained font/library-specific terms | `components/lvgl` |
| Espressif LVGL port 2.8.0~1 | Apache-2.0 | `components/esp_lvgl_port` |
| Espressif SPD2010 LCD driver 2.0.0~1 | Apache-2.0 | `components/esp_lcd_spd2010` |
| Espressif CO5300 LCD driver 2.1.0 | Apache-2.0 | `components/esp_lcd_co5300` |
| Espressif ST77916 LCD driver 2.0.2 | Apache-2.0 | `components/esp_lcd_st77916` |
| Espressif LCD touch 1.2.1 | Apache-2.0 | `components/esp_lcd_touch` |
| Espressif CST816S touch driver 1.1.1~1 | Apache-2.0 | `components/esp_lcd_touch_cst816s` |
| Waveshare CST9217 touch driver 1.0.4 | Apache-2.0 | `components/esp_lcd_touch_cst9217` |
| Espressif IO expander 1.2.1 | Apache-2.0 | `components/esp_io_expander` |
| Espressif TCA95xx 16-bit IO expander 2.0.2 | Apache-2.0 | `components/esp_io_expander_tca95xx_16bit` |
| Espressif button 4.2.0 | Apache-2.0 | `components/button` |
| Espressif knob 1.1.0 | Apache-2.0 | `components/knob` |
| Espressif CMake utilities 0.5.0 | Apache-2.0 | `components/cmake_utilities` |
| M5Stack M5PM1 1.0.7 | MIT | `components/m5pm1` |
| Announcement assets and Ogg parser | MIT | `components/mybot_platform/assets/locales`, `components/mybot_platform/src/common/ogg_opus_decoder.c` |
| Liberation Sans UI glyphs (OFL-1.1) | SIL Open Font License 1.1 | `components/mybot_platform/src/drivers/display/ili9342_lcd_font.inc`, `components/mybot_platform/src/drivers/display/OFL-1.1.txt` |
| Source Han Sans UI glyph subset | SIL Open Font License 1.1 | `components/mybot_platform/src/drivers/display/cores3_lvgl_font.c`, `components/mybot_platform/src/drivers/display/CORES3_FONT_LICENSE.txt` |
| Noto Color Emoji rasterized glyphs | SIL Open Font License 1.1 | `components/mybot_platform/assets/ui/noto_emoji`, `components/mybot_platform/src/drivers/display/cores3_ui_assets.c` |
| CoreS3 LVGL view (`1d5eeb2` reference) | MIT | `components/mybot_platform/src/drivers/display/cores3_lvgl_view.cc`, `components/mybot_platform/src/internal/cores3_lvgl_view.h` |
| Project-maintained shared LVGL panel adapters | Apache-2.0 unless file SPDX says otherwise | `components/mybot_platform/src/drivers/display/lvgl_st7789_lcd.cc`, `components/mybot_platform/src/drivers/display/dynamic_lvgl_lcd.cc`, `components/mybot_platform/src/drivers/display/vocat_lvgl_panel.c` |
| M5Stack CoreS3-derived implementation | MIT | Paths listed under MIT Attributions |
| ReSpeaker Flex XVF3800-derived implementation (`b060243`) | MIT | Paths listed under MIT Attributions |
| SenseCAP Watcher-derived implementation (`2b9b4e3`) | MIT | Paths listed under MIT Attributions |
| M5Stack StickS3-derived implementation (`2b9b4e3`) | MIT | Paths listed under MIT Attributions |
| Waveshare AMOLED 1.75/1.75C-derived implementation (`2b9b4e3`) | MIT | Paths listed under MIT Attributions |
| ESP-VoCat-derived implementation (`2b9b4e3`) | MIT | Paths listed under MIT Attributions |
| ESP-IDF | Apache-2.0 plus component-specific terms | External development SDK |

The built-in LVGL UI fonts retain their separate Montserrat OFL and Font Awesome terms at
`components/lvgl/scripts/built_in_font/font_license/Montserrat/OFL.txt` and
`components/lvgl/scripts/built_in_font/font_license/FontAwesome5/LICENSE.txt`.

The four local UI emoji are rasterized Noto Color Emoji font glyphs, supplied by the pinned
`78/noto-fonts` package and converted into constant pixels for this firmware. They retain
Google's copyright and the font's SIL OFL 1.1 terms in
`components/mybot_platform/assets/ui/noto_emoji/LICENSE.txt`. Source PNG hashes, Unicode glyphs,
and the conversion method are recorded in that directory's `SOURCES.json`.

## AOSL

The bundled AOSL license is based on Apache-2.0 and adds material deployment restrictions. Read
`components/aosl/LICENSE` before using, modifying, deploying, or redistributing firmware containing
AOSL. Do not describe the combined repository or firmware image as uniformly Apache-2.0.

## Agora RTSA

The repository contains an ESP32-S3 static RTSA library. No standalone license or NOTICE was
included in the supplied package. Possession of the binary is not evidence of redistribution
rights. Obtain and retain the applicable Agora license and written redistribution authorization
before publishing source archives, firmware images, mirrors, or releases containing this library.
If authorization is unavailable, exclude the library from public artifacts and require users to
supply an authorized package locally.

## MIT Attributions

The pinned esp-wifi-connect manifest declares MIT but its 3.2.2 archive omitted the license file.
`components/esp-wifi-connect/LICENSE` is the byte-identical MIT text later published by that
upstream project in commit `347682fa013b52f863052ad4b1a793ca3cabff17`.

The announcement assets and Ogg parsing implementation retain the MIT copyright of Shenzhen Xinzhi
Future Technology Co., Ltd. and Project Contributors. The assets originate from xiaozhi-esp32 and
their license is retained at `components/mybot_platform/assets/LICENSE.xiaozhi-esp32`.

The following M5Stack CoreS3 integration files contain substantial portions derived from
MIT-licensed xiaozhi-esp32 hardware and driver material. They use the MIT SPDX identifier and the
full permission text in `components/mybot_platform/assets/LICENSE.xiaozhi-esp32`:

- `components/mybot_platform/boards/m5stack-core-s3/`
- `components/mybot_platform/src/drivers/audio/cores3_codec_audio.c`
- `components/mybot_platform/src/drivers/display/ili9342_lcd.c`
- `components/mybot_platform/src/drivers/display/cores3_lcd_panel.c`
- `components/mybot_platform/src/internal/cores3_lcd_panel.h`
- `components/mybot_platform/src/drivers/display/cores3_lvgl_lcd.cc`
- `components/mybot_platform/src/drivers/input/ft6336_touch.c`

The CoreS3 LVGL view adapts the status/content/notification layout from the MIT-licensed
`github.com/junlon2006/xiaozhi-esp32` display implementation at commit
`1d5eeb2dd51cb315f98ef3c7d3f2b96bd2bbcf1d`. The original copyright and complete permission notice
are retained in `components/mybot_platform/src/drivers/display/CORES3_LVGL_VIEW_LICENSE.txt`.
Its generated Source Han Sans glyph subset retains Adobe's copyright and SIL OFL 1.1 text in
`components/mybot_platform/src/drivers/display/CORES3_FONT_LICENSE.txt`; it is not MIT-licensed.

The ReSpeaker Flex hardware mapping, AIC3104 initialization, XVF3800 control and button polling,
and I2S conversion are derived in part from the MIT-licensed
`github.com/qiuyanli1990/respeaker-flex-circle-Agora-mybot` reference at commit
`b06024382eb104c998aead4841e1df647193065b`. The following paths remain covered by its MIT terms;
the complete copyright and permission notice is retained in
`components/mybot_platform/assets/LICENSE.xiaozhi-esp32`:

- `components/mybot_platform/boards/respeaker-flex-xvf3800-circular4-xiao/`
- `components/mybot_platform/src/drivers/audio/xvf3800_audio.c`
- `partitions/v2/8m.csv`

The SenseCAP Watcher hardware mapping, codec integration, SPD2010 display integration, and partition
layout are derived in part from the MIT-licensed `github.com/junlon2006/xiaozhi-esp32` reference at
commit `2b9b4e3bf93c76fdfca1249ce0f7ed0bf546aaa0`. The following paths remain covered by its MIT terms;
the complete copyright and permission notice is retained in
`components/mybot_platform/assets/LICENSE.xiaozhi-esp32`:

- `components/mybot_platform/boards/sensecap-watcher/`
- `components/mybot_platform/src/drivers/audio/sensecap_codec_audio.c`
- `components/mybot_platform/src/drivers/display/spd2010_lcd.c`
- `partitions/v2/32m-sensecap.csv`

The M5Stack StickS3 hardware mapping, M5PM1 sequencing, ES8311 integration, and ST7789P3 display
configuration are derived in part from the MIT-licensed `github.com/junlon2006/xiaozhi-esp32`
reference at commit `2b9b4e3bf93c76fdfca1249ce0f7ed0bf546aaa0`. The following paths remain
covered by its MIT terms; the complete copyright and permission notice is retained in
`components/mybot_platform/assets/LICENSE.xiaozhi-esp32`:

- `components/mybot_platform/boards/m5stack-stick-s3/`
- `components/mybot_platform/src/drivers/audio/sticks3_es8311_audio.c`
- `components/mybot_platform/src/drivers/display/sticks3_st7789_lcd.c`

The vendored M5PM1 production sources retain the upstream M5Stack MIT license in
`components/m5pm1/LICENSE`.

The Waveshare ESP32-S3 Touch AMOLED 1.75 and 1.75C hardware mappings, AXP2101 sequence, CO5300
initialization, CST9217 integration, and codec routing are derived in part from the MIT-licensed
`github.com/junlon2006/xiaozhi-esp32` reference at commit
`2b9b4e3bf93c76fdfca1249ce0f7ed0bf546aaa0`. The following paths remain covered by its MIT terms;
the complete copyright and permission notice is retained in
`components/mybot_platform/assets/LICENSE.xiaozhi-esp32`:

- `components/mybot_platform/boards/esp32-s3-touch-amoled-1.75-common/`
- `components/mybot_platform/boards/esp32-s3-touch-amoled-1.75/`
- `components/mybot_platform/boards/esp32-s3-touch-amoled-1.75c/`
- `components/mybot_platform/src/drivers/audio/amoled175_codec_audio.c`
- `components/mybot_platform/src/drivers/display/amoled175_co5300_lcd.c`

The ESP-VoCat PCB detection, hardware mapping, ST77916 initialization, CST816S input behavior, and
codec routing are derived in part from the MIT-licensed `github.com/junlon2006/xiaozhi-esp32`
reference at commit `2b9b4e3bf93c76fdfca1249ce0f7ed0bf546aaa0`. The following paths remain
covered by its MIT terms; the complete copyright and permission notice is retained in
`components/mybot_platform/assets/LICENSE.xiaozhi-esp32`:

- `components/mybot_platform/boards/esp-vocat/`
- `components/mybot_platform/src/drivers/audio/vocat_codec_audio.c`
- `components/mybot_platform/src/drivers/display/vocat_st77916_lcd.c`

## Vendored SDK Notice

The original mybot SDK license and third-party notice are retained unchanged under
`third_party/mybot/`. That notice may describe packages used by other mybot targets; it is preserved
as part of the immutable SDK snapshot and does not redefine this firmware's dependency set.

Every component license file and file-level SPDX/copyright notice remains in force.
