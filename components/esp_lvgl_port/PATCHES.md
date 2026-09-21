# Local integration changes

The display/runtime subset is based on esp_lvgl_port 2.8.0~1, upstream
`espressif/esp-bsp` commit `d14ff131266bf1392efff88db72cb4638897507c`, path
`components/esp_lvgl_port`. The original Apache-2.0 copyright headers and
`license.txt` are retained.

Only the LVGL 9 runtime, display adapter, and their headers are included.
Local CMake selects sources through the hidden, board-derived `CONFIG_MYBOT_LVGL_UI`:
all display boards enable it and headless boards omit the runtime. Dependencies are
always declared for ESP-IDF component discovery. The component
does not invoke the component manager or fetch dependencies. Optional touch,
button, encoder, USB, PPA, and assembly extensions are not linked.

The two production C files contain these local corrections:

- Initialization validates configuration and rejects an already-owned runtime.
  A worker publishes initialization status after LVGL and the tick timer are
  ready, without retaining the caller's task handle for notification.
- Deinitialization requests stop, wakes the worker, and waits up to five
  seconds for LVGL cleanup. Only then does the owner release the timer, mutex,
  and event group. Failure preserves resources and blocks unsafe reinit.
  Allocation failures before task creation use synchronous cleanup.
- Tick callbacks use a static critical section and a generation token.
  Deinitialization disables ticks before deleting LVGL; a previously dispatched
  callback from an older instance cannot access a deleted mutex or a new LVGL
  instance.
- Display construction handles a missing display context, partially allocated
  draw buffers, failed `lv_display_create()`, and callback registration errors.
  Rollback deletes the display before freeing its context and owned buffers.
- A failed LCD bitmap submission completes the LVGL flush and reports the
  error, preventing LVGL from waiting for a callback that will never arrive.

The backend must stop its timers and refresh work, finish queued DMA transfers,
and quiesce LCD callbacks before display removal. The display-removal function
does not dereference the LCD IO/panel handle, so the backend may delete the IO
first to prevent late callbacks. Release the LVGL lock before calling
`lvgl_port_deinit()`. Lifecycle calls are serialized by the backend.

The integration is validated for the CoreS3 SPI display path. It does not claim
validation of the upstream RGB, DSI, monochrome, or software-rotation paths.
