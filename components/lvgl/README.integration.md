# LVGL integration

This is a production subset of LVGL 9.5.0, pinned to upstream commit
`85aa60d18b3d5e5588d7b247abf90198f07c8a63` from
<https://github.com/lvgl/lvgl>. Upstream source files are unchanged.

The import retains public/internal headers, core display and object code,
software drawing, text fonts, basic widgets, flex layout, and the built-in
binary decoder required by LVGL initialization. It excludes examples, demos,
tests, downloads, other platform implementations, optional image codecs, and
unused font bitmaps. Montserrat 14 and 32 include the upstream Font Awesome 5
symbol glyphs. Their licenses are retained under
`scripts/built_in_font/font_license/`; `LICENCE.txt` and `COPYRIGHTS.md` retain
the upstream library notices.

Local files:

- `CMakeLists.txt` registers sources only for `CONFIG_MYBOT_CORES3_LVGL_UI`.
  Dependencies remain declared during ESP-IDF's early component discovery.
- `Kconfig` supplies the draw-buffer alignment required by the display port.
- `lv_conf.h` selects RGB565, a single software draw unit, basic widgets,
  Montserrat 14/32, and disables themes, external decoders, and diagnostics.
- `local/lv_mem_psram.c` implements LVGL's custom allocator interface. Objects
  and draw scratch allocations use PSRAM with no internal-RAM fallback. An
  aligned allocation header tracks blocks so `lv_mem_deinit()` can reclaim
  remaining allocations. Allocation failures return `NULL`; failed realloc
  preserves the original allocation. All calls require the port's LVGL lock,
  except initialization/destruction while the runtime is quiescent.

`lv_mem_monitor()` reports the complete PSRAM heap, including other firmware
users. LCD DMA transfer buffers and the RTOS task/locks are allocated by the
port separately and are not covered by this allocator.
