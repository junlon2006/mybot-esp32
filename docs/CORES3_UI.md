# CoreS3 LVGL UI (shared view reference)

[English](CORES3_UI.md) | [简体中文](CORES3_UI.zh-CN.md)

`m5stack-core-s3` uses LVGL as its only display backend. The same semantic view is reused by all
supported panel adapters; see [Platform LVGL UI](PLATFORM_UI.md). The board profile automatically
sets the hidden `CONFIG_MYBOT_LVGL_UI` symbol. The previous cached renderer and its 16-page,
approximately 2.34 MiB PSRAM cache have been removed.

## Displayed information

The UI covers the ten existing workflow screens: startup, Wi-Fi provisioning,
Wi-Fi disconnected, service startup, pairing, pairing code, ready, conversation, failure, and
shutdown. Rounded status cards and local emoji distinguish workflow states. During conversations,
voiceprint registration stays visible alongside listening/thinking/speaking status. Static labels
follow the firmware's Chinese/English language setting. Pairing codes remain dynamic.

Pairing success, voiceprint registration, and network recovery can show a notification for up to two seconds
in the header when the corresponding state transition is observed. Notifications do not cover
pairing digits or replace the conversation voiceprint indicator. Repeated unchanged states do
not repeatedly trigger notifications. Moving to another workflow screen clears the previous notification.

Listening/thinking/speaking activity uses small, local dot/bar animations capped at 10 fps.
These indicate the SDK state and do not measure audio amplitude. Other screens remain static;
animation stops when leaving the active state.

The UI receives existing public SDK LCD content and board provisioning state. It does not add
conversation transcripts, cloud emotion messages, GIF animation, battery reporting, or new
network/SDK protocols. The bundled font covers the interface's static text rather than arbitrary
Chinese conversation text. Short touch still starts/stops a conversation; holding the screen for
three seconds still enters Wi-Fi provisioning.

## Theme and animation settings

These options appear under `mybot` in menuconfig for display boards:

| Setting | Default | Effect |
| --- | --- | --- |
| `CONFIG_MYBOT_LVGL_UI_LIGHT_THEME` | `n` | Dark theme by default; `y` selects the light theme |
| `CONFIG_MYBOT_LVGL_UI_ANIMATIONS` | `y` | Enables state activity animation; `n` keeps the indicators static |

Both choices are fixed at build time. No new touch gesture changes themes or animation settings.
Disabling activity animation preserves state changes, local emoji, and event notifications.

## Build and flash

Use ESP-IDF v5.5.2 and a separate build directory and sdkconfig:

```sh
idf.py -B build/cores3-lvgl-ui \
  -DMYBOT_BOARD=m5stack-core-s3 \
  -DSDKCONFIG=build/cores3-lvgl-ui/sdkconfig \
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults build
idf.py -B build/cores3-lvgl-ui -p <PORT> flash monitor
```

Chinese, video-off, dark theme, and enabled activity animation are the defaults. Keep separate
directories when comparing these variants:

| Variant | Suggested build directory | `SDKCONFIG_DEFAULTS` |
| --- | --- | --- |
| Chinese, video off | `build/cores3-lvgl-ui` | `sdkconfig.defaults` |
| English, video off | `build/cores3-lvgl-ui-en` | `sdkconfig.defaults;ci/en-us.defaults` |
| Chinese, video on | `build/cores3-lvgl-ui-video` | `sdkconfig.defaults;ci/video.defaults` |
| English, video on | `build/cores3-lvgl-ui-video-en` | `sdkconfig.defaults;ci/en-us.defaults;ci/video.defaults` |
| English, light theme, video off | `build/cores3-lvgl-ui-light-en` | `sdkconfig.defaults;ci/lvgl-ui-light.defaults;ci/en-us.defaults` |
| Chinese, static indicators, video on | `build/cores3-lvgl-ui-static-video` | `sdkconfig.defaults;ci/lvgl-ui-static.defaults;ci/video.defaults` |

Change both `-B` and `-DSDKCONFIG` to the chosen directory and use the matching defaults list.
All display boards include this UI automatically. Changing defaults does not overwrite an existing
generated sdkconfig; use a new sdkconfig or change theme/animation settings through `menuconfig`.
The light/static defaults files change only their respective option.

## Rendering and resources

The UI task runs on Core 1 at priority 1. LVGL objects use PSRAM; SPI transfers use a single
320 × 16 RGB565 buffer, or 10,240 bytes of internal DMA memory. Partial redraws update invalidated
regions instead of copying complete cached screens. No image or full-screen page cache is allocated.

Four 64 × 64 local emoji are stored as predecoded constant images in Flash, totaling approximately
64 KiB of pixel data. Rendering needs no PNG/GIF decoder or large decoded-image cache. Only the
small activity indicators are animated; the emoji images themselves are static.

State submissions copy the latest LCD content and wake the UI task. A 50-ms LVGL timer applies
pending content; rapid intermediate updates can be combined. SDK callbacks do not draw or wait
for SPI transfers. Board and SDK references share the display across provisioning and conversation.

LVGL introduces task, object, font, and rendering overhead. Removing the old page cache does not
by itself establish the total memory savings or lower CPU usage. Audio and camera code are
unchanged; their concurrent performance still needs measurement. The public SDK remains unchanged.

Lifecycle logs use `event=lcd` and `backend=lvgl`. Use CPU/heap statistics, lifecycle logs, and
visual checks to assess the UI. LVGL render/flush timing is not included in the playback statistics.

## Hardware validation

The CoreS3 LVGL UI has passed real-device testing. Hardware regression testing is still needed
after changes to the display stack. CI covers the four language/video combinations plus light-theme
and static-indicator variants. Successful builds and host checks do not establish the audible
or visual result on the device.

Verify the following before selecting it for a release:

- Check orientation, red/green/blue colors, small text, large pairing digits, and all ten screens.
- Test voiceprint and listening/thinking/speaking changes, including rapid transitions and hangup.
- Check both themes and animation settings. Confirm idle screens stop animating, event
  notifications expire within two seconds or on a workflow transition, and pairing codes and
  voiceprint status stay readable.
- Confirm short touch and three-second provisioning gestures, first-boot provisioning, Wi-Fi
  loss/recovery, and repeated SDK stop/start without stale screens or blank output.
- Listen for crackle during continuous full-duplex audio while the UI changes. Repeat with 1 fps
  video enabled, using the same volume and network.
- Record per-core CPU load, peak internal/PSRAM usage, minimum free heap and largest free blocks
  with `CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR`; repeat sessions to detect sustained memory growth.

Record firmware language, UI/video/theme/animation flags, volume, and complete serial logs with each result.
