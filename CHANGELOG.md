# Changelog

All notable changes to this project will be documented in this file. The project follows Semantic
Versioning and Conventional Commits.

## [Unreleased]

### Added

- Shared LVGL status UI for all eight display-board profiles, with Chinese/English text,
  pairing codes, voiceprint status, and listening/thinking/speaking indicators.
- LVGL light/dark themes, local state emoji, rounded status cards, persistent voiceprint
  indicators, and brief pairing/voiceprint/network notifications. Optional activity animations
  run at up to 5 fps; theme and animation choices are build settings.
- Provisioning screens display the actual device hotspot name. Names and connection hints
  scroll only when they exceed the available width, including with activity animations disabled;
  leaving provisioning stops the text scrolling.
- Optional CoreS3 offline audio playback comparison: identical local PCM through 60 ms
  timer-fed and continuous writes, with and without microphone capture.

- Optional CoreS3 GC0308 camera uplink: 320x240 software JPEG, capped at one frame per second,
  with bandwidth feedback, bounded capture waits, and shared-I2C-safe camera reset.

- ESP-IDF v5.5.2 project for the Zhengchen 1.54 TFT ESP32-S3 board.
- Zhengchen 1.54 TFT Wi-Fi Board profile with shared I2S, ST7789, Boot, volume-button, and
  power-hold support while leaving the ML307 UART pins unused.
- Waveshare ESP32-S3 Touch AMOLED 1.75 Board profile with AXP2101 power sequencing, CO5300 QSPI
  display, CST9217 touch input, and ES7210/ES8311 audio.
- Waveshare ESP32-S3 Touch AMOLED 1.75C Board profile with revision-specific audio MCLK, display
  reset, and touch-reset pins, sharing the established power, audio, display, and input drivers.
- Espressif ESP-VoCat Board profile with PCB V1.0/V1.2 runtime detection, ST77916 QSPI display,
  CST816S touch input, and ES7210/ES8311 full-duplex audio.
- Pinned ST77916 2.0.2 and CST816S 1.1.1~1 production components.
- Pinned CO5300 2.1.0, LCD touch 1.2.1, and CST9217 1.0.4 production components.
- mybot SDK 1.2.0 plus post-release fixes, Agora RTSA 1.10.1 and reference-counted AOSL integration.
- Wi-Fi provisioning/reconnect, NVS, verified HTTPS, I2S audio, buttons and LVGL status UI.
- Persistent 0-100 speaker volume using the Zhengchen board's software I2S gain path.
- Embedded Chinese and English Ogg/Opus pairing-code announcements decoded to PSRAM at runtime.
- Localized Wi-Fi provisioning prompts played after the configuration AP starts.
- M5Stack CoreS3 Board profile with ILI9342 display, FT6336 touch input, and
  ES7210/AW88298 audio support.
- M5Stack StickS3 Board profile with M5PM1 power sequencing, ES8311 audio, ST7789P3 status display,
  and main-button input.
- Real-device CoreS3 provisioning, bidirectional voice, 1 fps camera uplink, and LVGL UI validation.
- Agora RTM login and voice-print registration status displayed during active conversations.
- RTM channel subscription support paired with Agora RTSA 1.10.1 build 1270872.
- AOSL socket DSCP support required by the RTSA 1.10.1 network implementation.
- Voice-print registration-in-progress status shown immediately on the conversation screen.
- Optional per-core CPU and heap monitoring with playback timing and gap statistics,
  disabled by default.
- ReSpeaker Flex XVF3800 Circular-4 with XIAO ESP32S3 Board profile, including shared I2S audio,
  AIC3104 output initialization, XIAO Boot input, and XVF onboard-button polling.
- SenseCAP Watcher Board profile with ES8311/ES7243E audio, SPD2010 status display, rotary input,
  TCA9555 power sequencing, and a factory-data-preserving 32 MB partition layout.
- Target firmware CI with 22 configurations: all nine board profiles in both languages,
  CoreS3 video in both languages, and light-theme/static-indicator variants, using 60 ms audio.

### Changed

- Sync the MyBot SDK snapshot to `83fbcb0` (1.2.0 plus post-release fixes), serializing video
  control with shutdown, retaining announcements until RTC callbacks stop, and consolidating
  RTM-to-LCD indicator handling and device-service response parsing.
- Request CoreS3 DVP capture at most five times per second, holding capture buffers between
  samples so idle periods stop DMA and copying. The sensor retains its 20 fps timing and JPEG
  uplink remains capped at 1 fps. Increase the DVP DMA configuration to 16,384 bytes
  (15 KiB allocated for QVGA YUYV), and reduce shared UI activity animation to 5 fps.
- Count in-flight SPI callbacks across consecutive CoreS3 LVGL UI flushes so teardown waits
  for all pending transfers.

- Refresh the mybot SDK to commit `4ae239c` (1.2.0 plus post-release fixes): set AOSL to NOTICE
  at mybot startup, reduce routine RTM logging, and preserve the log level across RTSA initialization.
- Group MyBot SDK, AOSL, and Agora RTSA under `components/mybot_stack`, retaining independent
  ESP-IDF component names and registering them explicitly through `EXTRA_COMPONENT_DIRS`.
- Organize the platform as board profiles, platform registration, services, drivers, internal
  headers, and assets. Separate display panel lifecycle, LVGL adapters, and the shared view.
- Make LVGL the sole renderer on display boards, selected automatically by the board profile.
  ReSpeaker Flex remains headless; theme and activity-animation settings remain configurable.
- Set the ESP32 FreeRTOS tick rate to 1000 Hz so one operating-system tick is 1 ms.
- Sync the vendored mybot SDK to Unreleased commit `27324e7`, adding RTM channel subscription for
  voice-print status.
- Sync the vendored mybot SDK to v1.1.0 + Unreleased commit `e515f07`, including the serialized RTC
  MPQ implementation, pairing-state projection, media-session cleanup, and bounded service parsing.
- Sync the vendored mybot SDK to v1.2.0 (`674dcbf`), adding optional multimodal video uplink and
  mutually exclusive server-state LCD indicators while keeping video disabled on ESP32-S3 boards.
- Enforce the bundled ESP32-S3 RTSA package's fixed 60 ms cadence at component configuration time;
  non-60 ms packet durations now require a matching RTSA package.
- Update the AOSL baseline while retaining the ESP32-S3 FreeRTOS, PSRAM, and board-specific
  adaptations.
- Align the bilingual project, contribution, support, porting, and release documentation with the
  standalone ESP32 firmware scope.
- Decouple network provisioning from mybot startup. Connectivity is now a prerequisite, and holding
  Boot stops mybot before provisioning, shows the Wi-Fi setup screen, and restarts mybot after the
  station obtains an IP address.
- Select the target Board at compile time and separate common platform services, reusable drivers,
  and the Zhengchen board profile without changing its runtime behavior.
- Share the Zhengchen Board lifecycle between the ML307 and Wi-Fi profiles while keeping their
  compile-time hardware configuration isolated.
- Log volume-up and volume-down button presses in the ESP32-S3 platform layer.
- Shorten the provisioning AP SSID to `mybot-aabb`, using the first two STA MAC bytes.
- Sync the vendored mybot SDK to the v1.0.0 release (`117a44d`), retaining the ESP32-S3 RTC and
  HTTPS-only compatibility patches.

### Removed

- Legacy display renderers, their full-screen PSRAM caches and font, and the renderer-selection
  option. Panel initialization, power, and backlight handling remain in the hardware integration.
- Legacy `ui_stats` render/flush diagnostics; CPU, heap, and playback statistics remain available.

### Known limitations

- CoreS3 LVGL, including the theme/emoji stage, has passed real-device testing. The subsequent
  rollout and renderer cleanup across display boards, and the latest provisioning SSID/scrolling
  changes, still need hardware regression testing. The newer 5 fps DVP sampling and larger DMA
  buffers also require CPU, internal-memory, and concurrent-audio measurements. Conversation transcripts, cloud emotion
  messages, and GIF animation are not included.
- ML307/4G and wake words are not yet supported.
- Zhengchen Wi-Fi real-device validation, physical PSRAM-capacity confirmation, 16 kHz playback
  validation, charge/battery inputs, temperature monitoring, and power management are not yet
  complete.
- ESP-VoCat real-device validation for both PCB revisions is not yet complete. Battery reporting,
  IMU, PCB capacitive controls, SD card, LED, camera expansion, local AEC, reference audio,
  shutdown, and low-power behavior are not supported by the initial profile.
- Waveshare AMOLED 1.75 and 1.75C real-device validation, playback-reference input, local AEC,
  battery reporting, and low-power operation are not yet complete. The 1.75C profile does not
  support RTC, IMU, TF card, or TCA9554 and conservatively addresses 16 MB of Flash pending
  real-device capacity confirmation.
- ReSpeaker Flex requires separately flashed XVF3800 Circular-4 16 kHz I2S firmware; hardware
  validation, Linear-4 support, XVF firmware update, LED-ring status, and LCD output are not yet
  complete.
- SenseCAP Watcher real-device validation, camera, touch, LED, battery reporting, shutdown, and
  low-power behavior are not yet complete.
- M5Stack StickS3 real-device validation, GPIO12, IMU, infrared, battery reporting, shutdown, and
  low-power behavior are not yet complete.

### Fixed

- Register the nested runtime components so clean CI builds resolve `mybot_sdk`, AOSL, and RTSA.
- Preserve board-controlled backlights, track ST7789 DMA completion correctly, and retain resources
  for cleanup retries. Align CO5300 and SPD2010 partial refreshes to panel transfer requirements.
- Use the Chinese font for narrow-screen Chinese status labels instead of missing glyph boxes.
- Buffer PCM playback on all supported boards and feed I2S from a dedicated worker to
  decouple SDK timer callbacks from DMA refill timing. Preserve each board's native slot
  format and gain, bound startup buffering, drain finite prompts, and clear old PCM on stop.

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
