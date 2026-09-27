# Support

Use GitHub Issues for reproducible bugs and focused feature requests. Use GitHub Discussions when
enabled for integration questions.

> [English](SUPPORT.md) | [简体中文](SUPPORT.zh-CN.md)

Include:

- the exact repository commit and `idf.py --version` output;
- the board profile and whether the failure occurs during build, boot, provisioning, pairing, or RTC;
- sanitized serial logs with credentials, tokens, and device/customer data removed;
- Flash/PSRAM configuration and any hardware modifications;
- relevant configuration, including language, video, LVGL theme/animations, and resource monitoring;
- reproduction steps and the last known working version.

For display issues, include a screen photo and identify whether the failure was observed on hardware
or only in a host-side UI test. Every display board uses LVGL automatically; there is no legacy UI
backend to select. AtomEchoS3R and ReSpeaker Flex have no display. For provisioning issues, describe whether they
occur on first boot, after a long press, or when reconnecting with saved credentials.

The supported firmware network path is Wi-Fi on the Zhengchen 1.54 TFT ML307 and Wi-Fi profiles,
Xingzhi Cube 1.54 TFT Wi-Fi, Bread Compact Wi-Fi LCD, ESP-VoCat,
Waveshare ESP32-S3 Touch AMOLED 1.75, 1.75C, and 2.16,
M5Stack CoreS3, M5Stack StickS3, M5Stack AtomEchoS3R, ReSpeaker Flex XVF3800 Circular-4 with
XIAO ESP32S3, and SenseCAP Watcher. The
[CI workflow](.github/workflows/ci.yml) covers all thirteen profiles in both languages, with additional
CoreS3 video and UI variants. Build coverage does not establish real-device validation for a
particular firmware revision or hardware batch. ESP-VoCat reports must identify PCB V1.0 or V1.2
and include the runtime detection log.
The three AMOLED profiles must match the physical board and must not be cross-flashed. ML307/4G
reports are feature requests until a compatible network path is implemented and validated.
Bread Compact Wi-Fi LCD reports must identify the N16R8 module, ST7789 240x320 variant, and
actual wiring; other panel/I2S choices are outside this fixed profile. Chinese and English build
support has been validated; real-device testing remains pending.

The bundled RTSA package supports 60 ms audio frames. A build rejected after selecting 20 ms or
40 ms requires a matching RTSA package, not a change to the board driver.

This project has no support SLA. Questions about commercial Agora SDK or cloud services belong in
the applicable Agora support channel.
