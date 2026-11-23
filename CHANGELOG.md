# Changelog

All notable changes to this project will be documented in this file. The project follows Semantic
Versioning and Conventional Commits.

## [Unreleased]

### Added

- Three additional ESP32-S3 profiles: LilyGO T-Display S3 Pro with a 222×480 ST7796 panel,
  CST226SE touch and PDM capture; ATK DNESP32S3 Box with an 8-bit i80 ST7789 panel and ES8311
  audio; and Tudouzi as a headless Wi-Fi subset with ES8311/ES7210 audio. Added host coverage for
  the SY6970, XL9555/TCA9555 and AXP2101 startup sequences, CST226 direct-I2C input, i80 panel
  lifecycle and ST7796 command profile. All three passed local ESP-IDF v5.5.2 Chinese and English
  reconfigure/build/size/merge-bin validation; real-device validation remains pending.
- Five additional ESP32-S3 hardware families across six compile-time profiles: ATK DNESP32S3 with
  TCA9555 display power, Yunliao S3 as a Wi-Fi subset, LilyGO T-CameraPlus S3 V1.0/V1.1 and V1.2
  with separate standard-I2S/PDM audio paths, LilyGO T-Circle S3 with a 160×160 GC9D01N round
  panel, and Jiuchuan S3 with a 240×296 GC9309NA panel. Added host coverage for TCA9555 power
  setup and both custom controller profiles. All six passed local ESP-IDF v5.5.2 Chinese and
  English reconfigure/build/size/merge-bin validation; real-device validation remains pending.
- Three additional ESP32-S3 profiles: Zhengchen Cam with ST7789, ES8311/ES7210 and PCA9557 PA
  control; LabPlus Ledong V2 with a 320×172 ST7789-compatible panel, ES8388 and STM8 display
  reset; and LabPlus Mpython V3 with the same display/audio family and separate CS/backlight
  wiring. Added ES8388 device support and a host test for the bounded STM8 reset sequence. All
  three passed local ESP-IDF v5.5.2 Chinese and English
  reconfigure/build/size/merge-bin validation; real-device validation remains pending.
- Four additional ESP32-S3 profiles: Minsi K08 Dual with ST7789 and raw I2S, KEVIN Box 2 as a
  headless ES8311/ES7210 subset, Magiclick 2.5 with runtime GC9107/ST7735 panel selection, and
  ATK DNESP32S3 Box 3 with ST7789, dual codecs and AW9523B power/backlight/PA control. Added
  shared compact-panel and AW9523B drivers with host lifecycle tests. All four passed local
  ESP-IDF v5.5.2 Chinese and English reconfigure/build/size/merge-bin validation; real-device
  validation remains pending. The upstream AtomS3 Echo Base profile was not added because
  stock AtomS3 lacks the PSRAM required by the current MyBot/AOSL memory backend.
- KEVIN SP V4 Dev with a 240×280 ST7789 panel and ES8311 audio, plus DuChat X with a 128×160
  ST7789 panel, separate raw I2S audio and GPIO1 power hold. Both passed local ESP-IDF v5.5.2
  Chinese and English reconfigure/build/size/merge-bin validation; real-device validation
  remains pending.
- Six additional ESP32-S3 profiles: DOIT S3 AI Box with PDM capture, Magiclick 2.4 with
  NV3023, Movecall CuiCan/Moji with GC9A01, and Bread Compact Wi-Fi/HU-087 as headless raw-I2S
  subsets. Chinese and English local merged builds passed; real-device validation remains pending.
- Ten additional ESP32-S3 profiles: eight ST7789/ILI9341 boards using ES8311 or
  ES8311+ES7210 audio, followed by ST77916 LCD 1.85B/1.85C and SP 1.54 Muma. The batch adds
  compact 128×128 layout coverage, AXP2101/TCA9554 reset paths, a secondary board power rail,
  CST816S touch, and board-specific READMEs. Chinese and English local merged builds passed;
  real-device validation remains pending.
- Twenty-two additional ESP32-S3 board profiles covering NV3023, GC9A01, ILI9341, SH8601 and
  ST77916 panels; PCA9557/TCA9554, M5PM1/M5IOE1 and AXP2101 reset/power paths; FT5x06, GT911,
  CST816S-compatible touch and GPIO input; ES8311, ES7210, ES8374 and raw/PDM audio paths.
  `xingzhi-cube-0.96oled-wifi` is intentionally headless. All new profiles passed local
  ESP-IDF v5.5.2 Chinese and English reconfigure/build/size/merge-bin validation; real-device
  validation remains pending. Each new profile has its own concise board README linked from the
  board index.
- Shared compile-time board/hardware skeleton for board families whose lifecycle contracts match.
- Per-board hardware, input and operation documentation for the original profiles, with shared
  build/flash instructions and one validation matrix.
- CoreS3-only process-gray pixel-glasses UI, with 23 expressions, four 9 px terminal rows,
  a 10 px signature and fixed 32 px six-digit pairing codes on its 320x240 display.
  Automatic faces follow READY/conversation phases. Expressions affect only the LCD.
  `MYBOT_BOARD` selects the view automatically; other profiles retain their UI/hardware.
- Shared LVGL status UI for the other display-board profiles, with Chinese/English text,
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
- Xingzhi Cube 1.54 TFT Wi-Fi Board profile with separate microphone/speaker I2S, a 240x240
  ST7789 display, 60% PWM backlight, RTC GPIO21 power hold, and Boot/volume buttons.
- Bread Compact Wi-Fi LCD profile fixed to ESP32-S3 N16R8, a 240x320 ST7789 display,
  GPIO42 PWM backlight, separate microphone/speaker I2S, and Boot-only input. Chinese and English
  variants passed ESP-IDF v5.5.2 reconfigure/build/size/merge-bin validation.
- Waveshare ESP32-S3 Touch AMOLED 1.75 Board profile with AXP2101 power sequencing, CO5300 QSPI
  display, CST9217 touch input, and ES7210/ES8311 audio.
- Waveshare ESP32-S3 Touch AMOLED 1.75C Board profile with revision-specific audio MCLK, display
  reset, and touch-reset pins, sharing the established power, audio, display, and input drivers.
- Waveshare ESP32-S3 Touch AMOLED 2.16 Board profile with a 480x480 CO5300 panel, zero display
  gap, CST9217/Boot input, and shared AXP2101 and ES7210/ES8311 integration. Chinese and English
  variants passed ESP-IDF v5.5.2 reconfigure/build/size/merge-bin validation.
- Waveshare ESP32-S3 Touch AMOLED 1.8 V2 Board profile with a 368x448 CO5300 panel, TCA9554
  reset, AXP2101 power sequencing, CST816S/Boot input, and ES8311 audio.
- Waveshare ESP32-S3 Touch LCD 1.85 Board profile with a 360x360 ST77916 panel, panel-version
  selection, dual I2C buses, TCA9554 reset, separated I2S, right-slot microphone capture, and
  both-slot playback.
- GenjuTech S3 1.54 TFT Board profile with an inverting ST7789 display, ES8311 audio, Boot and
  volume buttons, and board-specific PA/display power coupling.
- Chinese and English variants for all three new profiles passed ESP-IDF v5.5.2
  reconfigure/build/size/merge-bin validation.
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
- M5Stack AtomEchoS3R headless Board profile with ES8311 audio and GPIO41 user-button input;
  hardware button mapping and 16 kHz audio remain unverified.
- Real-device CoreS3 provisioning, bidirectional voice, 1 fps camera uplink, and earlier
  status-card LVGL UI validation; these results do not validate the new pixel-glasses view.
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
- Target firmware CI for all board profiles in both languages, CoreS3 video and UI variants,
  using 60 ms audio. Current status is tracked in the [validation matrix](docs/VALIDATION.md).

### Changed

- Move the build baseline to ESP-IDF v6.1. Replace the removed built-in cJSON component with the
  pinned `espressif/cjson` managed dependency, migrate device-ID HMAC generation to PSA Crypto,
  and retain Newlib for compatibility with the prebuilt Agora RTSA archive. GenjuTech 1.54
  Chinese/English and CoreS3 video Chinese builds pass local
  reconfigure/build/size/merge-bin validation.

- Place eligible platform data and worker stacks in PSRAM across display, audio, video, input
  and optional diagnostics; select ESP-IDF external-stack support and PSRAM-preferred NVS cache
  automatically. Remove the PCM FIFO internal-memory fallback, retain internal DMA/kernel/atomic
  controls and the flash-writing main stack, and join suspended workers before freeing their
  PSRAM stacks with timeout-safe cleanup retries. Document exact capacities and regression
  commands in the [PSRAM memory policy](docs/PSRAM_MEMORY.md); current validation evidence
  remains in [VALIDATION.md](docs/VALIDATION.md).

- Extend the shared board skeleton with an optional secondary power rail for profiles that keep
  codec and system power on separate GPIOs.
- Centralize generic hardware sequencing, round-screen safe-area layout, dual-I2C selection,
  speaker PA control and display/audio capability derivation so additional profiles do not copy
  board lifecycle code.
- Centralize build and flash commands in the board index and current validation status in
  `docs/VALIDATION.md`; remove repeated status paragraphs, volatile build timings and per-board
  command copies from board and topic documentation.
- Sync the MyBot SDK snapshot to `8bae576` (1.3.0 plus upstream source style updates).
- Sync the MyBot SDK snapshot to `eebf185` (1.3.0 plus post-release fixes). The SDK completes
  framed HTTP responses without waiting for the peer to close, restores the pending pairing code
  after a Wi-Fi reconnect, and preserves stored credentials when a KV read fails. The firmware
  retries incomplete SDK shutdown before provisioning or tearing down the network.
- Refresh the SDK snapshot to `baf41ba` and register the new shared URL and HTTP-header sources.
  Adopt bounded URL validation and printable header checks, remove unused outbound RTM, media-mute
  and LCD emotion paths, and update the CoreS3 adapter and host tests to the current LCD contract.
- Update the local Chinese and English Wi-Fi provisioning prompts, retaining 20 ms mono Opus/Ogg
  and recording checksums and decoded frame counts.
- Refresh the SDK snapshot to `3f9a1ad` (version 1.3.0) and update all 22 Chinese/English pairing
  prompt and digit recordings from its PCM assets. Encode 20 ms mono Opus/Ogg for the existing
  firmware decoder and record source/output hashes.
- Sync the MyBot SDK snapshot to the v1.3.0 release (`58f0ef9`) and report version 1.3.0 from
  the ESP-IDF wrapper. Changes since the previous snapshot are release metadata and comments.
- Sync the MyBot SDK snapshot to `39d5ed1` (1.2.0 plus post-release fixes), accepting cloud
  `message.user` envelopes with absolute and relative volume commands. Remove MCP mute,
  expression, and bare-command parsing upstream; retain the platform's LCD emotion rendering.
- Replace only the CoreS3 status-card renderer with the BK7259 `dc4c854` display implementation,
  adapted to the existing ESP32 adapter/API. READY uses NEUTRAL; conversation entry uses
  SURPRISED for 300 ms plus its 250 ms enabled transition, continuous thinking becomes
  PROCESSING after three seconds, and MCP holds are 2/3/1.2/0.8 seconds plus transition.
  CoreS3 keeps a fixed process-gray skin; its legacy light-theme field remains compatible.
  Existing touch, audio, video auto-selection, power, memory and immutable SDK inputs are retained.
- Update the language-derived device-service addresses to `https://botstation.shengwang.cn/api`
  for zh-CN and `https://botstation.agora.io/api` for en-US, matching the SDK's regional guidance.
- Sync the MyBot SDK snapshot to `e7793eb` (1.2.0 plus post-release fixes), adding cloud
  volume/mute commands and moving status polls and stop notifications off the control worker.
  Register the new device-command source in ESP-IDF. The shared workflow view on other boards
  continues to ignore optional emotion indicators; CoreS3 maps them to pixel expressions.
- Sync the MyBot SDK snapshot to `0643978` (1.2.0 plus post-release fixes), preventing control
  queue stalls, clearing pending starts on re-pairing, discarding stale announcement PCM, and
  hardening HTTP and JSON boundaries. The RTC queue limit increases from 64 to 1000 pending calls.
- Direct pairing-code entry to the web console and scroll the instruction on narrow displays.
- Select the device API region from the firmware language: zh-CN uses Shanghai (sh2), and en-US
  uses Singapore (sg3).
- Sync the MyBot SDK snapshot to `83fbcb0` (1.2.0 plus post-release fixes), serializing video
  control with shutdown, retaining announcements until RTC callbacks stop, and consolidating
  RTM-to-LCD indicator handling and device-service response parsing.
- Request CoreS3 DVP capture at most five times per second, holding capture buffers between
  samples so idle periods stop DMA and copying. The sensor retains its 20 fps timing and JPEG
  uplink remains capped at 1 fps. Use an 8,192-byte DVP DMA configuration
  (7.5 KiB allocated for QVGA YUYV), and reduce shared UI activity animation to 5 fps.
- Count in-flight SPI callbacks across consecutive CoreS3 LVGL UI flushes so teardown waits
  for all pending transfers.

- Refresh the mybot SDK to commit `4ae239c` (1.2.0 plus post-release fixes): set AOSL to NOTICE
  at mybot startup, reduce routine RTM logging, and preserve the log level across RTSA initialization.
- Group MyBot SDK, AOSL, and Agora RTSA under `components/mybot_stack`, retaining independent
  ESP-IDF component names and registering them explicitly through `EXTRA_COMPONENT_DIRS`.
- Organize the platform as board profiles, platform registration, services, drivers, internal
  headers, and assets. Separate display panel lifecycle, LVGL adapters, and the shared view.
- Make LVGL the sole renderer on display boards, selected automatically by the board profile.
  AtomEchoS3R and ReSpeaker Flex remain headless; theme and activity-animation settings remain
  configurable.
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

Current build coverage, real-device results and unsupported features are tracked only in the
[validation matrix](docs/VALIDATION.md).

### Fixed

- Move CoreS3's 7,168-byte LVGL task stack to PSRAM through the existing port's stack-capability
  setting, preserving the internal display DMA stripe. Require external-stack support in the board
  profile and log `ui_stack_bytes=7168 ui_stack=psram` during display initialization. This saves
  7 KiB of internal stack storage; real-device audio/video/UI,
  provisioning and volume-persistence NVS regressions remain to be verified.
- Reduce CoreS3's contiguous internal DMA buffer requirement from 15 KiB to 7.5 KiB with a
  project Kconfig default override without changing vendored driver code. Log the configured DMA
  bound and free/largest internal DMA memory before stream startup and when streaming does not
  start. A device still failed with 13,187 bytes free but a largest block of 5,120 bytes; this
  footprint reduction alone did not resolve the allocation failure. The smaller buffer increases
  DMA events per active frame and requires concurrent audio/UI validation.
- Restrict batch-builder cleanup to dedicated, owned generated directories. Reject repository
  ancestors, source directories, symlink components, traversal and overlapping roots before any
  cleanup; preserve unmarked existing data and anchor child deletion to directory descriptors.
- Keep AMOLED 2.16 preparation idempotent and retry pending input/hardware cleanup before a
  new initialization attempt.
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
