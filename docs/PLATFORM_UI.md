# Platform LVGL UI

[English](PLATFORM_UI.md) | [简体中文](PLATFORM_UI.zh-CN.md)

The shared LVGL workflow view is an optional display backend. It is selected at build time with
`CONFIG_MYBOT_LVGL_UI`; the existing board renderer remains the default. The SDK LCD contract,
audio path, camera path, and touch/key ownership do not change.

## Current adapters

| Board | Panel and logical size | LVGL adapter | Input behavior |
| --- | --- | --- | --- |
| `zhengchen-1.54tft-ml307` | ST7789, 240 × 240 | shared ST7789 adapter | existing buttons |
| `zhengchen-1.54tft-wifi` | ST7789, 240 × 240 | shared ST7789 adapter | existing buttons |
| `m5stack-core-s3` | ILI9342, 320 × 240 | CoreS3 adapter | existing FT6336 gestures |
| `m5stack-stick-s3` | ST7789P3, 135 × 240 | shared ST7789 adapter | existing main button |
| `esp-vocat` | ST77916, 360 × 360 | VoCat adapter | existing CST816S/Boot path |
| `esp32-s3-touch-amoled-1.75` | CO5300, 466 × 466 | shared AMOLED adapter | existing touch/key path |
| `esp32-s3-touch-amoled-1.75c` | CO5300, 466 × 466 | shared AMOLED adapter | existing touch/key path |
| `sensecap-watcher` | SPD2010, 412 × 412 | shared Watcher adapter | existing encoder path |

The ReSpeaker Flex profile has no LCD and does not enable this option. The ML307, Wi-Fi, and
audio/video protocols are independent of the display backend.

## Shared behavior

The view receives semantic SDK LCD content: the ten workflow screens, pairing code, voiceprint
indicator, and listening/thinking/speaking state. Coordinates are derived from each panel's
logical width and height; the view does not assume the CoreS3 320 × 240 geometry. Small local
state images are predecoded into Flash; no runtime PNG/GIF decoder or full-screen cache is added.

Touch and button events remain in each board's input driver. Selecting LVGL does not add a new
gesture, alter provisioning, or change the audio/video lifecycle. The backend uses one partial
RGB565 DMA stripe and a dedicated LVGL task; the panel adapter owns reset, offsets, color order,
backlight, and teardown.

## Build examples

Use a separate build directory for every board and language. For an LVGL build, append
`ci/lvgl-ui.defaults` to the board defaults:

```sh
idf.py -B build/esp-vocat-lvgl \
  -DMYBOT_BOARD=esp-vocat \
  -DSDKCONFIG=build/esp-vocat-lvgl/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;ci/lvgl-ui.defaults" build
```

The same option is used by the other adapters. Theme and activity-animation settings are build
options; no runtime theme gesture is introduced. Keep the original board build in a separate
directory to compare its renderer.

## Validation status

All listed LVGL paths pass the ESP-IDF v5.5.2 local English/Chinese build matrix. CI adds LVGL
variants for every adapter listed above, including selected video/theme combinations. These are
build checks only. No new board is claimed to have passed real-device color, touch, audio,
provisioning, or teardown validation. Complete hardware testing must cover all workflow screens,
orientation and colors, touch/buttons, Wi-Fi recovery, repeated mybot stop/start, full-duplex
audio, and memory/CPU measurements.
