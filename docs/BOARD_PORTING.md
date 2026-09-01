# Board porting

> [English](BOARD_PORTING.md) | [简体中文](BOARD_PORTING.zh-CN.md)

mybot firmware selects exactly one Board at compile time. Runtime board detection is intentionally
not supported because the IDF target, Flash size and mode, PSRAM mode, and partition table must be
known before components are configured.

## Layout

```text
components/mybot_platform/
  include/mybot_board.h       Board metadata and registration entry points
  src/common/                 Board-independent ESP-IDF services
  src/drivers/                Reusable hardware-family drivers
  src/core/                   Selected-board registration
  boards/<board-id>/          Board descriptor, pins, sources, and sdkconfig defaults
```

The vendored mybot core remains board-independent. Every Board provides a process-lifetime
`mybot_board_t`, owns the product network prerequisite and provisioning lifecycle, and registers one
complete `mybot_platform_descriptor_t`. The mybot Wi-Fi adapter attaches to the already-connected
network and monitors runtime link changes; it does not own initial provisioning or shut down the
Board network. KV storage, key input, capture, and playback are required by the SDK; hardware
volume, HTTPS, LCD, announcements, and wake words are optional unless the active product
configuration requires them.

## Build profile

The root `MYBOT_BOARD` CMake cache variable selects a profile before `project()` configures ESP-IDF.
The profile defines:

- `MYBOT_BOARD_TARGET`
- `MYBOT_BOARD_SDKCONFIG_DEFAULTS`
- `MYBOT_BOARD_INCLUDE_DIR`
- `MYBOT_BOARD_REQUIRED_CONFIGS`
- `MYBOT_BOARD_FORBIDDEN_CONFIGS` (optional)
- `MYBOT_BOARD_PARTITION_TABLE`
- `MYBOT_BOARD_SOURCES`
- `MYBOT_BOARD_REQUIRES`

Use a separate build directory and sdkconfig for every Board. The build cache rejects changing
`MYBOT_BOARD` in place, and component configuration rejects stale Flash, PSRAM, or partition
settings that do not satisfy the selected profile.

This is required even for Boards sharing the same ESP-IDF target. For example,
`zhengchen-1.54tft-ml307` uses 16 MB Flash and Octal PSRAM,
`m5stack-core-s3` uses 16 MB Flash and Quad PSRAM, and
`m5stack-stick-s3` and `respeaker-flex-xvf3800-circular4-xiao` use 8 MB Flash and Octal PSRAM.
`sensecap-watcher` uses 32 MB Flash with 32-bit addressing and Octal PSRAM.

```sh
idf.py -B build/<board-id> \
  -DMYBOT_BOARD=<board-id> \
  -DSDKCONFIG=build/<board-id>/sdkconfig build
```

Board defaults own Flash, PSRAM, and partition settings. Product-wide settings remain in
`sdkconfig.defaults`; target-wide settings remain in `sdkconfig.defaults.<target>`.

## Adding a Board

1. Add `boards/<board-id>/board.cmake`, `board.c`, `board_config.h`, and `sdkconfig.defaults`.
2. Add the Board ID and profile mapping to `boards/boards.cmake`. Board selection is never changed
   from menuconfig; `-DMYBOT_BOARD=<board-id>` is the only user-facing source of truth.
3. Reuse common services and an existing typed driver where the hardware contract matches.
4. Implement the Board network lifecycle so `ensure_network()` returns only after usable IP
   connectivity, `provision_wifi()` returns only after provisioning reconnects the station, and
   `shutdown_network()` performs final network teardown.
5. Keep power sequencing and unusual codec, display, touch, or input behavior inside the Board or a
   hardware-family driver; do not add ESP-IDF details to `third_party/mybot`.
6. Preserve the SDK audio boundary: 16 kHz, mono, signed 16-bit PCM, with frame counts rather than
   byte counts.
7. Add an isolated CI build and size report, then record real-device provisioning, HTTPS, RTC,
   bidirectional audio, input, display, hangup, and repeated start/stop validation.

LCD `indicators` are non-exclusive overlays on the semantic base screen. Render recognized bits
without replacing the underlying workflow label and ignore unknown bits.
`MYBOT_LCD_INDICATOR_VP_REGISTERED` is currently meaningful only on
`MYBOT_LCD_SCREEN_IN_CONVERSATION`. The supported ESP32-S3 displays show voice-print registration
in progress when the conversation screen has no registered indicator, then replace it with the
registered status when `MYBOT_LCD_INDICATOR_VP_REGISTERED` is set.

When capture and playback share one physical I2S peripheral, keep the peripheral and codec state in
a ref-counted Board driver. The SDK initializes and destroys capture and playback independently,
and the provisioning prompt can open playback before the first SDK start.

An input source that must request provisioning while mybot is stopped may be owned by the Board for
the process lifetime. In that case, the SDK key operations only attach and detach the event callback:
the Board must prevent callbacks after detach and wait for any callback already in flight.

The ReSpeaker Flex profile is an example of a hardware audio front end rather than a raw microphone
codec. Its XVF3800 must run separately flashed Circular-4 16 kHz, two-channel I2S firmware and owns
AEC, beamforming, AGC, and noise suppression. The ESP32-S3 uses I2S slave mode and converts the
selected 32-bit capture slot to the SDK's mono signed-16 boundary. Keep Cloud AEC disabled for this
profile to prevent double processing. A build does not validate the XVF firmware, clock direction,
channel routing, or acoustic performance; those require real-device tests.

SenseCAP Watcher requires a dedicated partition table. Its `nvsfactory` partition starts at
`0x9000`, is 200 KiB long, and must never be replaced with the common Board NVS layout. Keep mybot's
normal `nvs` in a separate partition, never use `erase-flash`, and document a factory-partition
backup before first flashing. Standard `idf.py flash` does not write `nvsfactory`.

The initial Watcher audio path configures ES8311 and ES7243E directly for 16 kHz, mono signed-16
PCM. If real hardware cannot sustain that clock configuration, keep the physical link at 24 kHz and
add stateful resampling in the Board driver without changing the SDK boundary. SPD2010 QSPI updates
must align the X start and width to four pixels; full-width 412-pixel strips satisfy this constraint.

M5Stack StickS3 uses M5PM1 G2 as the shared process-lifetime power rail for its ST7789P3 display and
ES8311 codec. G3 belongs to the playback lifecycle and must remain off until the codec is ready to
play. Its physical I2S link is stereo while the SDK boundary remains 16 kHz mono signed-16 PCM:
capture selects the microphone slot and playback duplicates mono samples into both slots. The
135 x 240 display uses a (52, 40) panel offset, so every label and pairing code must be measured
against the 135-pixel logical width. The GPIO11 input remains Board-owned so a long press can request
provisioning while mybot is stopped.
