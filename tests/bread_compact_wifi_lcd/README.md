# Bread Compact Wi-Fi LCD Host Tests

```sh
sh tests/bread_compact_wifi_lcd/run.sh
```

The runner compiles production code against the actual Bread board profile and
host ESP-IDF implementations. It reuses the ST7789/shared LVGL lifecycle test
body with the 240x320 profile, and adds separate board and single-button input
executables.

Coverage includes display and input initialization failures, retained button
handles after cleanup failures, retry without overwriting handles, repeated
SDK attach/detach, in-flight callback completion before detach returns, detached
long-press provisioning, PWM failures, and idempotent board preparation. Button
construction accepts GPIO0 only; GPIO and RTC operations are rejected by the
board mocks.

Host tests do not establish physical panel colors/orientation, power timing,
microphone slot wiring, audio quality, or Wi-Fi behavior.
