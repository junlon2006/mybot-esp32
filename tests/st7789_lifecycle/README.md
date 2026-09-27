# ST7789 Lifecycle Host Tests

Run from the repository root:

```sh
sh tests/st7789_lifecycle/run.sh
```

The harness compiles the production ST7789 panel and shared LVGL adapter with
host implementations of the ESP-IDF and LVGL APIs. It uses the
`xingzhi-cube-1.54tft-wifi` board configuration.

Coverage includes initialization failures, partially returned constructor
handles, retained resources after cleanup failures, repeated attach/detach and
open/close, failed DMA submissions, callback completion before resource release,
and bounded teardown when a transfer callback never arrives. A deterministic
clock reaches the production one-second timeout without sleeping.

A separate executable links the production Xingzhi board lifecycle with LCD,
GPIO, RTC, LEDC, button, and network mocks. Each scenario runs in a fresh process
to cover hardware preparation failures, successful retries, LCD cleanup after
render/input initialization failures, idempotent preparation, and provisioning
callback/return ordering. The process-lifetime power hold remains asserted.

These tests validate ownership and error paths. They do not validate physical
panel colors, orientation, power timing, brightness, audio quality, or Wi-Fi.
