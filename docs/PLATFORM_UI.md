# Platform LVGL UI

[English](PLATFORM_UI.md) | [简体中文](PLATFORM_UI.zh-CN.md)

All supported display boards use the shared LVGL workflow view. The board profile automatically
sets the hidden `CONFIG_MYBOT_LVGL_UI` symbol; there is no renderer selection switch or legacy
renderer. Do not add a manual `CONFIG_MYBOT_LVGL_UI` override to sdkconfig. The SDK LCD contract,
audio path, camera path, and touch/key ownership do not change.

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

The AtomEchoS3R and ReSpeaker Flex profiles have no LCD and do not include LVGL. The ML307 and
Wi-Fi variants, along with the audio/video protocols, are independent of the display backend.

## Shared behavior

The view receives semantic SDK LCD content: the ten workflow screens, pairing code, voiceprint
indicator, and listening/thinking/speaking state. Coordinates are derived from each panel's
logical width and height; the view does not assume the CoreS3 320 × 240 geometry. Small local
state images are predecoded into Flash; no runtime PNG/GIF decoder or full-screen cache is added.

The provisioning screen shows the actual device SoftAP SSID (`mybot-xxxx`) in the center and a
Chinese/English connection hint below. Its title and hint scroll in a loop only when wider than
their labels; short text stays still, and scrolling stops on exit. This text scrolling remains
available when `CONFIG_MYBOT_LVGL_UI_ANIMATIONS` disables state activity animation.

The pairing-code screen directs users to enter the code in the web console. Its instruction
scrolls only when wider than the footer label and stops scrolling on exit.

On screens narrower than 240 pixels (currently StickS3), Chinese labels retain the 20 px bilingual
font; English status, notification, and ordinary labels use 10 px text. The provisioning SSID
uses 20 px in both languages and scrolls if needed. Pairing codes select a 32, 20, or 10 px font
to fit, with tighter spacing for long codes on narrow screens. The built-in Chinese subset covers
the UI labels, not arbitrary conversation text.

Touch and button events remain in each board's input driver. The UI does not add a new
gesture, alter provisioning, or change the audio/video lifecycle. The backend uses one partial
RGB565 DMA stripe of 16 rows (`width × 16 × 2` bytes) and a dedicated LVGL task on Core 1 at
priority 1. Objects and drawing scratch space use PSRAM; the DMA stripe uses internal memory.
CoreS3's stripe is 10,240 bytes and StickS3's is 4,320 bytes, separate from task stacks and driver
allocations. The panel adapter owns reset, offsets, color order, backlight, and teardown.

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

The earlier CoreS3 LVGL UI passed real-device testing; the LVGL-only cleanup, provisioning SSID
display/scrolling, and narrow-screen font changes still need hardware regression. CI covers all
ten board profiles in both languages, CoreS3 video in both languages, and two theme/animation
variants (24 configurations).
These are build checks only. No new board is claimed to have passed real-device color, touch, audio,
provisioning, or teardown validation. Complete hardware testing must cover all workflow screens,
orientation and colors, Chinese/English small-screen text, SSID and hint scrolling with activity
animation disabled, touch/buttons, Wi-Fi recovery, repeated mybot stop/start, full-duplex audio,
and memory/CPU measurements.
