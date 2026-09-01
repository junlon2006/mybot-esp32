# Release Checklist

> [English](RELEASING.md) | [简体中文](RELEASING.zh-CN.md)

## Prepare

- [ ] Choose a Semantic Version and update `PROJECT_VER` in `CMakeLists.txt` and `CHANGELOG.md`.
- [ ] Confirm README, board support, known limitations, and configuration documentation are current.
- [ ] Review all dependency revisions, bundled licenses, and `THIRD_PARTY_NOTICES.md`.
- [ ] Confirm written rights to redistribute every bundled binary, especially Agora RTSA.
- [ ] Confirm no credential, token, private endpoint, customer data, generated sdkconfig, or NVS data
      is tracked.

## Verify

```sh
. /path/to/esp-idf/export.sh
test "$(idf.py --version)" = "ESP-IDF v5.5.2"
idf.py -B build/release \
  -DMYBOT_BOARD=zhengchen-1.54tft-ml307 \
  -DSDKCONFIG=build/release/sdkconfig build
idf.py -B build/release size
git diff --check
```

- [ ] CI passes for all supported boards, both languages, and 20/40/60 ms packet durations.
- [ ] Both OTA slots retain sufficient rollback headroom.
- [ ] Format, SPDX, whitespace, and commit-message checks pass.
- [ ] On each release board, test provisioning, reconnect, pairing, HTTPS, bidirectional audio,
      voice-print status, hangup, repeated start/stop, and reboot persistence.
- [ ] Negative-test invalid CA, hostname mismatch, TLS timeout, missing NVS values, and Wi-Fi loss.
- [ ] Confirm logs and release archives contain no credentials.
- [ ] Confirm release configurations enable the intended NVS/Flash encryption and Secure Boot policy.
- [ ] For Zhengchen Wi-Fi, confirm 16 MB Flash and the physical PSRAM capacity from startup logs;
      test GPIO2 power hold, ST7789 output, Boot and volume buttons, and verify GPIO11/GPIO12 remain
      unused. Validate 16 kHz capture/playback speed, pitch, stability, and full-duplex interaction
      against the hardware's 24 kHz speaker-output requirement.
- [ ] For Waveshare AMOLED 1.75, use only the non-C profile; test USB and battery boot, AXP2101
      rails and charger settings, 8 MB PSRAM detection, MCLK GPIO42, LCD reset GPIO39, touch reset
      GPIO40, optional TCA9554 detection, ES7210 primary-mic routing, 16 kHz full-duplex audio, PA
      pop/noise, CO5300 gap/colors/alignment/brightness, CST9217 orientation, and provisioning from
      both touch and Boot while mybot is stopped.
- [ ] For Waveshare AMOLED 1.75C, use only the C profile; test USB and battery boot, AXP2101 rails,
      8 MB PSRAM detection, MCLK GPIO16, LCD reset GPIO1, touch reset GPIO2, absence of TCA9554
      probing, primary-mic routing, full-duplex audio, PA noise, display/touch, and provisioning.
      Confirm the detected Flash capacity before expanding beyond the safe 16 MB partition layout.
- [ ] Negative-test both Waveshare AMOLED revisions by attempting to configure the other revision's
      profile, and verify the release process never cross-flashes their firmware artifacts.
- [ ] For SenseCAP Watcher, back up and checksum the 200 KiB `nvsfactory` region before first flash;
      verify normal flashing leaves it unchanged and never publish an `erase-flash` procedure.
- [ ] For M5Stack StickS3, test USB and battery boot, M5PM1 G2/G3 sequencing, speaker pop/noise,
      16 kHz capture slot routing, ST7789P3 offsets/colors, and GPIO11 provisioning while mybot is
      stopped.

## Publish

- [ ] Create an annotated `v<version>` tag matching `PROJECT_VER`.
- [ ] Attach source and firmware artifacts only after third-party authorization review.
- [ ] Include `LICENSE`, `THIRD_PARTY_NOTICES.md`, the changelog, partition layout, and checksums.
- [ ] List known limitations and distinguish build validation from real-hardware validation.
