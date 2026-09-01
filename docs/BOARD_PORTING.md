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

This is required even for Boards sharing the same ESP-IDF target. For example, the
`zhengchen-1.54tft-ml307` and `zhengchen-1.54tft-wifi` profiles use 16 MB Flash and configure Octal
PSRAM at 80 MHz, `esp-vocat` uses 16 MB Flash and Octal PSRAM at 80 MHz, the
`esp32-s3-touch-amoled-1.75` and `esp32-s3-touch-amoled-1.75c` profiles use a safe 16 MB Flash
profile and 8 MB Octal PSRAM, `m5stack-core-s3` uses 16 MB Flash and Quad PSRAM, and
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

The Zhengchen ML307 and Wi-Fi profiles share `boards/zhengchen-1.54tft-common/board.c` because their
display, I2S, buttons, and power-hold contracts are identical. Profile-specific headers retain the
Board identity and optional modem pins; the Wi-Fi profile must not configure or access GPIO11 and
GPIO12. Its physical PSRAM capacity must be confirmed from the startup log even though the profile
selects Octal PSRAM at 80 MHz.

The initial Zhengchen Wi-Fi path keeps the SDK boundary at 16 kHz mono signed-16 PCM and uses a
16 kHz mono 32-bit left-slot physical I2S link. The original speaker configuration uses 24 kHz
output, so real-device acceptance must cover playback speed, pitch, stability, capture, and
full-duplex interaction. If 24 kHz output is required, add stateful 16-to-24 kHz resampling inside
the Board driver and keep the SDK contract unchanged. GPIO9 charge status, GPIO8 battery ADC,
temperature monitoring, sleep, and low-power behavior remain outside the initial profile.

The `esp-vocat` profile supports PCB V1.0 and V1.2 under one compile-time Board because they share
the same target, storage, partition, and component configuration. This is runtime hardware-revision
detection, not runtime Board selection. Board preparation creates native I2C0 on GPIO2/GPIO1, drives
GPIO48 low, waits 50 ms, and probes ES8311 at `0x18`. A successful probe freezes the V1.0 map;
otherwise it drives GPIO48 high and retries for V1.2. If both probes fail, preparation must fail.
No audio, display, or conflicting GPIO may initialize before the revision map is frozen.

V1.0 selects audio DIN GPIO15, PA GPIO4, and active-low LCD reset GPIO3. V1.2 selects DIN GPIO3,
PA GPIO15, and active-high reset GPIO47. GPIO9 enables the peripheral rail at a low output level.
The profile uses USB Serial/JTAG because GPIO43/GPIO44 overlap the default UART0 console and the
Board LED/backlight wiring.

ESP-VoCat capture and playback use identical two-slot standard-I2S clocks at the SDK's 16 kHz rate;
capture extracts MIC1/slot0 and playback duplicates mono samples into both TX slots. Keep TX enabled
during capture-only operation to supply shared clocks. If a release device cannot sustain 16 kHz,
put stateful 24-to-16 and 16-to-24 kHz conversion inside this driver without changing the mybot
contract.

The 360 x 360 ST77916 round display uses its Board-specific initialization sequence and full-width
RGB565 DMA strips. CST816S input uses GPIO10 any-edge interrupts and reads the controller only after
an event; a 20 ms timer runs only while pressed to detect the 3-second hold. The profile disables
CST816S ID reads because supported touch-firmware batches do not consistently answer that register.
CST816S and Boot input remain Board-owned, allowing long-press provisioning while mybot is stopped.
Battery reporting, BMI270, PCB capacitive controls, SD, LED behavior, camera
expansion, local AEC, reference audio, shutdown, and low-power behavior remain outside the initial
profile.

The `esp32-s3-touch-amoled-1.75` and `esp32-s3-touch-amoled-1.75c` profiles share Board lifecycle,
AXP2101, audio, CO5300, and CST9217 implementations, but keep independent identities and pin
headers. The original revision uses audio MCLK GPIO42, LCD reset GPIO39, touch reset GPIO40, and an
optional TCA9554 probe at `0x20`; 1.75C uses GPIO16, GPIO1, and GPIO2 respectively and never probes
TCA9554. These profiles must not be cross-flashed. A Board-owned I2C0 bus initializes AXP2101 power
before CO5300, CST9217, and codec clients attach.

Both defaults use 16 MB QIO Flash, 8 MB Octal PSRAM at 80 MHz, and `partitions/v2/16m.csv`. The
1.75C product page describes 32 MB Flash while the reference firmware targets 16 MB and available
hardware materials conflict. Keep the conservative address space until startup detection confirms
the release hardware; only then add and validate a larger partition profile. RTC, IMU, TF card,
battery reporting, and automatic sleep remain outside both initial profiles, and 1.75C does not
probe or depend on TCA9554.

Its ES7210 and ES8311 share one I2S0 clock domain. The initial implementation selects only the
primary microphone and configures identical two-slot 16 kHz standard-I2S TX/RX, extracts the left
capture slot, duplicates mono playback into both slots, and keeps Cloud AEC enabled. Capture-only
operation must keep TX running to supply MCLK/BCLK/WS. The playback-reference input and local AEC
are not exposed through the mybot audio contract.

CO5300 QSPI rendering uses full-width 466 x 16 RGB565 DMA strips with a (6, 0) panel gap. Every
transfer region keeps even start/end pixel boundaries; a full framebuffer and LVGL are intentionally
excluded. CST9217 and Boot input remain Board-owned so either can request provisioning while mybot
is stopped. Each profile is a build target only until power, audio-slot, display, and touch behavior
are verified on its matching release device.

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
