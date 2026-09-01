# Changelog

All notable changes to this project will be documented in this file. The project follows Semantic
Versioning and Conventional Commits.

## [Unreleased]

### Added

- ESP-IDF v5.5.2 project for the Zhengchen 1.54 TFT ESP32-S3 board.
- mybot 1.0.0, Agora RTSA 1.10.1 and reference-counted AOSL integration.
- Wi-Fi provisioning/reconnect, NVS, verified HTTPS, I2S audio, buttons and ST7789 status UI.
- Persistent 0-100 speaker volume using the Zhengchen board's software I2S gain path.
- Embedded Chinese and English Ogg/Opus pairing-code announcements decoded to PSRAM at runtime.
- Localized Wi-Fi provisioning prompts played after the configuration AP starts.
- M5Stack CoreS3 Board profile with ILI9342 display, FT6336 touch input, and
  ES7210/AW88298 audio support.
- Real-device CoreS3 provisioning and bidirectional voice validation.
- Agora RTM login and voice-print registration status displayed during active conversations.
- RTM channel subscription support paired with Agora RTSA 1.10.1 build 1270872.
- AOSL socket DSCP support required by the RTSA 1.10.1 network implementation.
- Voice-print registration-in-progress status shown immediately on the conversation screen.
- ReSpeaker Flex XVF3800 Circular-4 with XIAO ESP32S3 Board profile, including shared I2S audio,
  AIC3104 output initialization, XIAO Boot input, and XVF onboard-button polling.
- Target firmware CI for all supported boards, both languages, and supported audio packet times.

### Changed

- Set the ESP32 FreeRTOS tick rate to 1000 Hz so one operating-system tick is 1 ms.
- Sync the vendored mybot SDK to Unreleased commit `27324e7`, adding RTM channel subscription for
  voice-print status.
- Update the AOSL baseline while retaining the ESP32-S3 FreeRTOS, PSRAM, and board-specific
  adaptations.
- Align the bilingual project, contribution, support, porting, and release documentation with the
  standalone ESP32 firmware scope.
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

- ML307/4G, wake words, battery and power management are not yet supported.
- ReSpeaker Flex requires separately flashed XVF3800 Circular-4 16 kHz I2S firmware; hardware
  validation, Linear-4 support, XVF firmware update, LED-ring status, and LCD output are not yet
  complete.

### Fixed

- Correct the CoreS3-derived file licenses, add the missing esp-wifi-connect MIT text, and remove
  stale Component Registry cache checksums from locally adapted components.
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
