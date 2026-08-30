# Releasing

1. Update `PROJECT_VER` in the root `CMakeLists.txt` and the changelog together. The firmware reads
   this version from the generated ESP-IDF app descriptor.
2. Confirm the vendored revisions and notices are current.
3. Use ESP-IDF v5.5.2 to perform a clean build and inspect `idf.py size` output.
4. Verify the app fits both OTA slots with sufficient rollback headroom.
5. Run format, commit-message, SPDX and `git diff --check` validation.
6. On the target board, validate first-boot provisioning, reconnect, pairing, HTTPS, RTC join,
   bidirectional audio, hangup, repeated start/stop and reboot persistence.
7. Negative-test invalid CA, hostname mismatch, TLS timeout, missing NVS values and Wi-Fi loss.
8. Confirm logs and NVS dumps do not disclose device, RTC or Wi-Fi credentials.
9. Publish an annotated `v<version>` tag with `LICENSE`, `THIRD_PARTY_NOTICES.md`, checksums,
   firmware binaries, partition layout and known limitations.

Do not describe a successful compile as hardware validation. ML307/4G builds must not be released as
supported until Agora has a verified lwIP-compatible path on that network.
