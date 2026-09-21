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

LVGL is the only renderer for all display boards; the ReSpeaker Flex profile remains headless.
`CONFIG_MYBOT_LVGL_UI` is derived from the selected board and is not a user-facing switch.
New panel work should put hardware lifecycle in `panels/` and use an adapter under `adapters/`
when LVGL needs controller-specific handles. CO5300, SPD2010, and ST77916 implement the shared
`mybot_display_panel_open()` / `mybot_display_panel_close()` interface directly in `panels/`.
