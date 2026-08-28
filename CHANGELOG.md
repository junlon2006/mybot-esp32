# Changelog

All notable changes to this project will be documented in this file. The project follows Semantic
Versioning and Conventional Commits.

## [Unreleased]

### Added

- ESP-IDF v5.5.2 project for the Zhengchen 1.54 TFT ESP32-S3 board.
- mybot 1.0.0, Agora RTSA 1.10.0 and reference-counted AOSL integration.
- Wi-Fi provisioning/reconnect, NVS, verified HTTPS, I2S audio, buttons and ST7789 status UI.
- Persistent 0-100 speaker volume using the Zhengchen board's software I2S gain path.
- Embedded Chinese and English Ogg/Opus pairing-code announcements decoded to PSRAM at runtime.
- Localized Wi-Fi provisioning prompts played after the configuration AP starts.
- M5Stack CoreS3 Board profile with ILI9342 display, FT6336 touch input, and
  ES7210/AW88298 audio support.
- Target firmware CI plus the upstream mybot sanitizer, coverage and documentation jobs.

### Changed

- Set the ESP32 FreeRTOS tick rate to 1000 Hz so one operating-system tick is 1 ms.
- Decouple network provisioning from mybot startup. Connectivity is now a prerequisite, and holding
  Boot stops mybot before provisioning, shows the Wi-Fi setup screen, and restarts mybot after the
  station obtains an IP address.
- Select the target Board at compile time and separate common platform services, reusable drivers,
  and the Zhengchen board profile without changing its runtime behavior.
- Log volume-up and volume-down button presses in the ESP32-S3 platform layer.
- Shorten the provisioning AP SSID to `mybot-aabb`, using the first two STA MAC bytes.
- Sync the vendored mybot SDK to the v1.0.0 release (`117a44d`), retaining the ESP32-S3 RTC and
  HTTPS-only compatibility patches.

### Known limitations

- Physical-device and bidirectional RTC validation is pending.
- ML307/4G, wake words, battery and power management are not yet supported.

### Fixed

- Start SNTP after Wi-Fi obtains an IP address so Agora logs use synchronized UTC timestamps
  instead of the Unix epoch.
- Initialize lwIP and the default event loop before AOSL creates its internal socket-based signal
  pipe.
- Preserve the requested AOSL thread stack size on ESP-IDF instead of dividing the byte count by
  `sizeof(StackType_t)`.
- Increase the lwIP socket budget so mybot and Agora can open network sockets after AOSL creates
  its MPQ wakeup pipes.
- Make ESP32-S3 AOSL millisecond sleeps block for at least one FreeRTOS tick so MPQ teardown cannot
  starve the task that releases its final queue reference.
- Validate DNS request sizes and wait for the provisioning DNS worker before releasing its server.
