# Display driver layout

Display code is organized by responsibility:

- `panels/` owns controller and bus setup. It exposes only the internal panel lifecycle needed by a renderer.
- `renderers/lvgl/` contains the shared semantic status view, fonts, and image assets.
- `adapters/lvgl/` connects panel profiles to the shared LVGL runtime.
- `assets/licenses/` keeps licenses beside generated display assets.

Board profiles include `display.cmake` and declare a panel and LVGL adapter through
the helper functions there. Common LVGL sources are added once by `mybot_display_add_lvgl_sources()`.
The renderer consumes semantic `mybot_lcd_content_t`; it does not own Wi-Fi, audio, or SDK state.
For the provisioning title it copies the active SoftAP name through the Wi-Fi service's internal
snapshot getter. The getter does not acquire the provisioning-operation lock or call the display.
LVGL scrolls overflowing provisioning labels and stops their animations when leaving that screen.

LVGL is the only renderer for all display boards; AtomEchoS3R and ReSpeaker Flex remain headless.
`CONFIG_MYBOT_LVGL_UI` is derived from the selected board and is not a user-facing switch.
New panel work should put hardware lifecycle in `panels/` and use an adapter under `adapters/`
when LVGL needs controller-specific handles. ST7789, CO5300, SPD2010, and ST77916 implement the shared
`mybot_display_panel_open()` / `mybot_display_panel_close()` interface directly in `panels/`.
The new ST7789 panel reads SPI mode and offsets from the board profile and delegates backlight to
the board. Xingzhi Cube uses this panel with the shared adapter: SPI3 mode 3, a 240 x 240 display
with no offset, and LEDC backlight at 5 kHz, 13-bit resolution, and 60% duty. Existing Zhengchen and
StickS3 profiles retain `st7789_lvgl_adapter.cc` and their own hardware configuration.
The independent `co5300/co5300_480_panel.c` variant is reserved for AMOLED 2.16's 480 x 480,
zero-gap geometry. The 1.75 profiles retain `co5300/co5300_panel.c` and their 466 x 466, (6, 0)
geometry. Both variants use the shared LVGL panel adapter and even pixel boundaries on both axes;
Chinese and English 2.16 build support has been validated, while real-device validation is pending.
