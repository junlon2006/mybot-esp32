# 支持

可复现的 bug 与明确的特性请求请使用 GitHub Issues。GitHub Discussions 开启后，集成问题请
在那里讨论。

> [English](SUPPORT.md) | [简体中文](SUPPORT.zh-CN.md)

请提供：

- 准确的仓库 commit 与 `idf.py --version` 输出；
- Board profile，以及问题发生在构建、启动、配网、配对还是 RTC 阶段；
- 已删除凭据、token 和设备/客户数据的串口日志；
- Flash/PSRAM 配置和任何硬件改动；
- 复现步骤与最后一个正常版本。

当前支持征辰 1.54 TFT ML307 与 Wi-Fi profile、ESP-VoCat、Waveshare ESP32-S3 Touch
AMOLED 1.75 与 1.75C profile、M5Stack CoreS3、M5Stack StickS3，以及搭配 XIAO ESP32S3
的 ReSpeaker Flex XVF3800 Circular-4 和 SenseCAP Watcher 的 Wi-Fi 固件路径。征辰 Wi-Fi、
ESP-VoCat 与两个 Waveshare AMOLED profile 已覆盖构建验证，但仍需完成真机验证。
ESP-VoCat 问题必须注明 PCB V1.0 或 V1.2，并附带运行时探测日志。两个 AMOLED 硬件版本必须
使用匹配的 profile，禁止交叉烧录。在实现并验证兼容网络路径之前，ML307/4G 相关问题按特性
请求处理。

本项目不提供支持 SLA。Agora 商业 SDK 或云服务问题应通过对应的 Agora 支持渠道处理。
