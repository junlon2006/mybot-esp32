# Board porting

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
`mybot_board_t` and registers one complete `mybot_platform_descriptor_t`. Wi-Fi, KV storage, key
input, capture, and playback are required by the SDK; hardware volume, HTTPS, LCD, announcements,
and wake words are optional unless the active product configuration requires them.

## Build profile

The root `MYBOT_BOARD` CMake cache variable selects a profile before `project()` configures ESP-IDF.
The profile defines:

- `MYBOT_BOARD_TARGET`
- `MYBOT_BOARD_SDKCONFIG_DEFAULTS`
- `MYBOT_BOARD_INCLUDE_DIR`
- `MYBOT_BOARD_REQUIRED_CONFIGS`
- `MYBOT_BOARD_PARTITION_TABLE`
- `MYBOT_BOARD_SOURCES`
- `MYBOT_BOARD_REQUIRES`

Use a separate build directory and sdkconfig for every Board. The build cache rejects changing
`MYBOT_BOARD` in place, and component configuration rejects stale Flash, PSRAM, or partition
settings that do not satisfy the selected profile.

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
4. Keep power sequencing and unusual codec, display, touch, or input behavior inside the Board or a
   hardware-family driver; do not add ESP-IDF details to `third_party/mybot`.
5. Preserve the SDK audio boundary: 16 kHz, mono, signed 16-bit PCM, with frame counts rather than
   byte counts.
6. Add an isolated CI build and size report, then record real-device provisioning, HTTPS, RTC,
   bidirectional audio, input, display, hangup, and repeated start/stop validation.
