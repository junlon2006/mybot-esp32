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

- [ ] CI passes for both boards, both languages, and 20/40/60 ms packet durations.
- [ ] Both OTA slots retain sufficient rollback headroom.
- [ ] Format, SPDX, whitespace, and commit-message checks pass.
- [ ] On each release board, test provisioning, reconnect, pairing, HTTPS, bidirectional audio,
      voice-print status, hangup, repeated start/stop, and reboot persistence.
- [ ] Negative-test invalid CA, hostname mismatch, TLS timeout, missing NVS values, and Wi-Fi loss.
- [ ] Confirm logs and release archives contain no credentials.
- [ ] Confirm release configurations enable the intended NVS/Flash encryption and Secure Boot policy.

## Publish

- [ ] Create an annotated `v<version>` tag matching `PROJECT_VER`.
- [ ] Attach source and firmware artifacts only after third-party authorization review.
- [ ] Include `LICENSE`, `THIRD_PARTY_NOTICES.md`, the changelog, partition layout, and checksums.
- [ ] List known limitations and distinguish build validation from real-hardware validation.
