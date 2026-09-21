# Platform LVGL UI

[English](PLATFORM_UI.md) | [简体中文](PLATFORM_UI.zh-CN.md)

All supported display boards use the shared LVGL workflow view. The board profile automatically
sets the hidden `CONFIG_MYBOT_LVGL_UI` symbol; there is no renderer selection switch or legacy
renderer. The SDK LCD contract, audio path, camera path, and touch/key ownership do not change.

## Current adapters

| Board | Panel and logical size | LVGL adapter | Input behavior |
| --- | --- | --- | --- |
| `zhengchen-1.54tft-ml307` | ST7789, 240 × 240 | shared ST7789 adapter | existing buttons |
| `zhengchen-1.54tft-wifi` | ST7789, 240 × 240 | shared ST7789 adapter | existing buttons |
| `m5stack-core-s3` | ILI9342, 320 × 240 | CoreS3 adapter | existing FT6336 gestures |
| `m5stack-stick-s3` | ST7789P3, 135 × 240 | shared ST7789 adapter | existing main button |
| `esp-vocat` | ST77916, 360 × 360 | shared panel adapter | existing CST816S/Boot path |
| `esp32-s3-touch-amoled-1.75` | CO5300, 466 × 466 | shared panel adapter | existing touch/key path |
| `esp32-s3-touch-amoled-1.75c` | CO5300, 466 × 466 | shared panel adapter | existing touch/key path |
| `sensecap-watcher` | SPD2010, 412 × 412 | shared panel adapter | existing encoder path |

The ReSpeaker Flex profile has no LCD and does not include LVGL. The ML307, Wi-Fi, and
audio/video protocols are independent of the display backend.

## Shared behavior

The view receives semantic SDK LCD content: the ten workflow screens, pairing code, voiceprint
indicator, and listening/thinking/speaking state. Coordinates are derived from each panel's
logical width and height; the view does not assume the CoreS3 320 × 240 geometry. Small local
state images are predecoded into Flash; no runtime PNG/GIF decoder or full-screen cache is added.

Touch and button events remain in each board's input driver. The UI does not add a new
gesture, alter provisioning, or change the audio/video lifecycle. The backend uses one partial
RGB565 DMA stripe and a dedicated LVGL task; the panel adapter owns reset, offsets, color order,
backlight, and teardown.

## Build examples

Use a separate build directory for every board and language. No extra UI defaults are required:

```sh
idf.py -B build/esp-vocat-lvgl \
  -DMYBOT_BOARD=esp-vocat \
  -DSDKCONFIG=build/esp-vocat-lvgl/sdkconfig \
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults build
```

Theme and activity-animation settings remain build options; no runtime theme gesture is
introduced. Append `ci/lvgl-ui-light.defaults` for the light theme or `ci/lvgl-ui-static.defaults`
for static activity indicators. Use a new sdkconfig when changing defaults, because defaults do
not override values saved in an existing sdkconfig.

## Validation status

The CoreS3 LVGL UI has passed real-device testing. CI covers the nine board profiles in both
languages, CoreS3 video in both languages, and two theme/animation variants (22 configurations).
These are build checks only. No new board is claimed to have passed real-device color, touch, audio,
provisioning, or teardown validation. Complete hardware testing must cover all workflow screens,
orientation and colors, touch/buttons, Wi-Fi recovery, repeated mybot stop/start, full-duplex
audio, and memory/CPU measurements.
