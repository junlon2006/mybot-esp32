# Support

Use GitHub Issues for reproducible bugs and focused feature requests. Use GitHub Discussions when
enabled for integration questions.

> [English](SUPPORT.md) | [简体中文](SUPPORT.zh-CN.md)

Include:

- the exact repository commit and `idf.py --version` output;
- the board profile and whether the failure occurs during build, boot, provisioning, pairing, or RTC;
- sanitized serial logs with credentials, tokens, and device/customer data removed;
- Flash/PSRAM configuration and any hardware modifications;
- reproduction steps and the last known working version.

The supported firmware network path is Wi-Fi on the Zhengchen 1.54 TFT ML307 and Wi-Fi profiles,
ESP-VoCat, Waveshare ESP32-S3 Touch AMOLED 1.75 and 1.75C profiles, M5Stack CoreS3, M5Stack StickS3,
ReSpeaker Flex XVF3800 Circular-4 with XIAO ESP32S3, and SenseCAP Watcher. The Zhengchen Wi-Fi,
ESP-VoCat, and both Waveshare AMOLED profiles have build coverage but still require real-device
validation. ESP-VoCat reports must identify PCB V1.0 or V1.2 and include the runtime detection log.
The two AMOLED revisions require their matching profile and must not be cross-flashed. ML307/4G
reports are feature requests until a compatible network path is implemented and validated.

This project has no support SLA. Questions about commercial Agora SDK or cloud services belong in
the applicable Agora support channel.
