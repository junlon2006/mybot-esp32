# Release Checklist

> [English](RELEASING.md) | [简体中文](RELEASING.zh-CN.md)

## Prepare

- [ ] Choose a Semantic Version and update `PROJECT_VER` in `CMakeLists.txt` and `CHANGELOG.md`.
- [ ] Confirm README, board support, known limitations, and configuration documentation are current.
- [ ] Review all dependency revisions, bundled licenses, and `THIRD_PARTY_NOTICES.md`.
- [ ] Verify the MyBot snapshot, AOSL sources, and RTSA headers/library in
      `components/mybot_stack` match `VENDORED_SOURCES.md`; keep the SDK snapshot unchanged
      except for an explicit upstream synchronization.
- [ ] Confirm written rights to redistribute every bundled binary, especially Agora RTSA.
- [ ] Confirm no credential, token, private endpoint, customer data, generated sdkconfig, or NVS data
      is tracked.

## Verify

```sh
. /path/to/esp-idf/export.sh
test "$(idf.py --version)" = "ESP-IDF v5.5.2"
idf.py -B build/release \
  -DMYBOT_BOARD=zhengchen-1.54tft-ml307 \
  -DSDKCONFIG=build/release/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;ci/ptime60.defaults" build
idf.py -B build/release size
git diff --check
```

- [ ] All 22 firmware builds in the [CI workflow](../.github/workflows/ci.yml) pass: nine boards
      in both languages, two additional CoreS3 video builds, one CoreS3 light-theme build, and one
      CoreS3 video build with conversation animations disabled. All use 60 ms audio frames.
- [ ] Confirm unsupported 20 ms and 40 ms settings still fail configuration with the bundled RTSA.
- [ ] Record firmware builds, host-side tests, and real-device results separately. Host layout or
      lifecycle tests do not validate panel transfers, power sequencing, or acoustic performance.
- [ ] Both OTA slots retain sufficient rollback headroom.
- [ ] Format, SPDX, whitespace, and commit-message checks pass.
- [ ] On each release board, test provisioning, reconnect, pairing, HTTPS, bidirectional audio,
      voice-print status, hangup, repeated start/stop, and reboot persistence.
- [ ] On display boards, verify the sole LVGL renderer on the actual panel, including colors,
      orientation, readable pairing/voice-print status, and backlight. Check that provisioning
      displays the active device hotspot name, long text scrolls, and scrolling stops after leaving
      provisioning. Test first boot, button-triggered provisioning, and connection-failure retry.
      ReSpeaker Flex remains headless. See [platform UI](PLATFORM_UI.md).
- [ ] For CoreS3 video, validate GC0308 colors/orientation, server JPEG reception, at most 1 fps,
      bandwidth adaptation, internal DMA memory, concurrent audio/UI, repeated conversations,
      provisioning while streaming, and no callback after successful stop. See
      [CoreS3 video](CORES3_VIDEO.md).
- [ ] Negative-test invalid CA, hostname mismatch, TLS timeout, missing NVS values, and Wi-Fi loss.
- [ ] Confirm logs and release archives contain no credentials.
- [ ] Confirm release configurations enable the intended NVS/Flash encryption and Secure Boot policy.
- [ ] For Zhengchen Wi-Fi, confirm 16 MB Flash and the physical PSRAM capacity from startup logs;
      test GPIO2 power hold, ST7789 output, Boot and volume buttons, and verify GPIO11/GPIO12 remain
      unused. Validate 16 kHz capture/playback speed, pitch, stability, and full-duplex interaction
      against the hardware's 24 kHz speaker-output requirement.
- [ ] For ESP-VoCat, test PCB V1.0 and V1.2 separately. Verify GPIO48 detection, revision-specific
      DIN/PA/LCD-reset pins and reset polarity, GPIO9 peripheral power, USB Serial/JTAG logging,
      detected Flash/PSRAM capacity, the ST77916 initialization/colors/round edges/backlight,
      CST816S press/release interrupt edges without startup ID reads, Boot provisioning while mybot
      is stopped, primary-mic slot routing, 16 kHz full-duplex audio, PA pop/noise, Cloud AEC,
      volume persistence, and repeated start/stop.
      Record any display-touch batch that requires the vendor touch-firmware update.
- [ ] For Waveshare AMOLED 1.75, use only the non-C profile; test USB and battery boot, AXP2101
      rails and charger settings, 8 MB PSRAM detection, MCLK GPIO42, LCD reset GPIO39, touch reset
      GPIO40, optional TCA9554 detection, ES7210 primary-mic routing, 16 kHz full-duplex audio, PA
      pop/noise, CO5300 gap/colors/alignment/brightness, CST9217 orientation, and provisioning from
      both touch and Boot while mybot is stopped.
- [ ] For Waveshare AMOLED 1.75C, use only the C profile; test USB and battery boot, AXP2101 rails,
      8 MB PSRAM detection, MCLK GPIO16, LCD reset GPIO1, touch reset GPIO2, absence of TCA9554
      probing, primary-mic routing, full-duplex audio, PA noise, display/touch, and provisioning.
      Confirm the detected Flash capacity before expanding beyond the safe 16 MB partition layout.
- [ ] Verify an existing build directory rejects a change of `MYBOT_BOARD`, and keep separate
      artifacts for both Waveshare AMOLED revisions. A clean build accepts either profile; it
      cannot detect the connected board's revision. Match the physical revision before flashing.
- [ ] For SenseCAP Watcher, back up and checksum the 200 KiB `nvsfactory` region before first flash;
      verify normal flashing leaves it unchanged and never publish an `erase-flash` procedure.
- [ ] For M5Stack StickS3, test USB and battery boot, M5PM1 G2/G3 sequencing, speaker pop/noise,
      16 kHz capture slot routing, ST7789P3 offsets/colors, 60% PWM backlight, and GPIO11 provisioning
      while mybot is stopped.

## Publish

- [ ] Create an annotated `v<version>` tag matching `PROJECT_VER`.
- [ ] Attach source and firmware artifacts only after third-party authorization review.
- [ ] Include `LICENSE`, `THIRD_PARTY_NOTICES.md`, the changelog, partition layout, and checksums.
- [ ] List known limitations and distinguish build validation from real-hardware validation.
