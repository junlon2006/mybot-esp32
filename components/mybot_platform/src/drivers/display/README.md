# Display driver layout

Display code is organized by responsibility:

- `panels/` owns controller and bus setup. It exposes only the internal panel lifecycle needed by a renderer.
- `renderers/legacy/` contains the existing board-specific text/framebuffer renderers.
- `renderers/lvgl/` contains the shared semantic status view, fonts, and image assets.
- `adapters/lvgl/` connects a panel profile to the shared LVGL runtime. Board-specific providers live here.
- `assets/licenses/` keeps licenses beside generated display assets.

Board profiles include `display.cmake` and declare a panel, legacy renderer, or LVGL adapter through
the helper functions there. Common LVGL sources are added once by `mybot_display_add_lvgl_sources()`.
The renderer consumes semantic `mybot_lcd_content_t`; it does not own Wi-Fi, audio, or SDK state.

The legacy renderer files still contain their original transport code while migration preserves
runtime behavior. New panel work should put hardware lifecycle in `panels/` and use an adapter under
`adapters/` when LVGL needs controller-specific handles.
