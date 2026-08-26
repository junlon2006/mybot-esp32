# Changelog

All notable changes to this project will be documented in this file. The project follows Semantic
Versioning and Conventional Commits.

## [Unreleased]

### Added

- ESP-IDF v5.5.2 project for the Zhengchen 1.54 TFT ESP32-S3 board.
- mybot 1.0.0, Agora RTSA 1.10.0 and reference-counted AOSL integration.
- Wi-Fi provisioning/reconnect, NVS, verified HTTPS, I2S audio, buttons and ST7789 status UI.
- Target firmware CI plus the upstream mybot sanitizer, coverage and documentation jobs.

### Changed

- Sync the vendored mybot SDK to the v1.0.0 release (`117a44d`), retaining the ESP32-S3 RTC and
  HTTPS-only compatibility patches.

### Known limitations

- Physical-device and bidirectional RTC validation is pending.
- ML307/4G, pairing announcements, wake words, battery and power management are not yet supported.

### Fixed

- Initialize lwIP and the default event loop before AOSL creates its internal socket-based signal
  pipe.
- Preserve the requested AOSL thread stack size on ESP-IDF instead of dividing the byte count by
  `sizeof(StackType_t)`.
- Increase the lwIP socket budget so mybot and Agora can open network sockets after AOSL creates
  its MPQ wakeup pipes.
