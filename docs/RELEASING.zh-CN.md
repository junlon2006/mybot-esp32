# 发布检查清单

> [English](RELEASING.md) | [简体中文](RELEASING.zh-CN.md)

## 准备

- [ ] 确定语义化版本，并同步更新 `CMakeLists.txt` 中的 `PROJECT_VER` 与 `CHANGELOG.md`。
- [ ] 确认 README、板卡支持、已知限制与配置文档符合当前实现。
- [ ] 审查全部依赖版本、随附许可证与 `THIRD_PARTY_NOTICES.md`。
- [ ] 确认拥有重新分发每个内置二进制的书面权利，尤其是 Agora RTSA。
- [ ] 确认未跟踪凭据、token、私有服务地址、客户数据、生成的 sdkconfig 或 NVS 数据。

## 验证

```sh
. /path/to/esp-idf/export.sh
test "$(idf.py --version)" = "ESP-IDF v5.5.2"
idf.py -B build/release \
  -DMYBOT_BOARD=zhengchen-1.54tft-ml307 \
  -DSDKCONFIG=build/release/sdkconfig build
idf.py -B build/release size
git diff --check
```

- [ ] 所有支持板卡、两种语言及 20/40/60 ms 音频包长的 CI 全部通过。
- [ ] 两个 OTA 分区保留足够的回滚空间。
- [ ] 格式、SPDX、空白与提交信息检查通过。
- [ ] 在每个发布板卡上验证配网、重连、配对、HTTPS、双向音频、声纹状态、挂断、重复启停
      与重启持久化。
- [ ] 负向测试无效 CA、hostname 不匹配、TLS 超时、NVS 值缺失与 Wi-Fi 丢失。
- [ ] 确认日志与发布归档不包含任何凭据。
- [ ] 确认发布配置采用预期的 NVS/Flash encryption 与 Secure Boot 策略。
- [ ] 对征辰 Wi-Fi，通过启动日志确认 16 MB Flash 与实际 PSRAM 容量；验证 GPIO2 电源保持、
      ST7789、Boot 与音量按键，并确认 GPIO11/GPIO12 保持未使用。结合硬件的 24 kHz 扬声器
      输出要求，验证 16 kHz 采集/播放的速度、音调、稳定性与全双工交互。
- [ ] 对 ESP-VoCat，分别验证 PCB V1.0 与 V1.2。确认 GPIO48 探测、各版本 DIN/PA/LCD reset
      引脚与 reset 极性、GPIO9 外设电源、USB Serial/JTAG 日志、实际 Flash/PSRAM 容量、
      ST77916 初始化/颜色/圆屏边缘/背光、CST816S 按下/释放中断且启动不读取 ID、mybot 停止
      期间 Boot 配网、主麦 slot 路由、16 kHz 全双工、PA 爆音/噪声、Cloud AEC、音量持久化
      和反复启停。对需要厂商触摸固件升级的屏幕批次单独记录。
- [ ] 对 Waveshare AMOLED 1.75，仅使用非 C profile；验证 USB 与电池启动、AXP2101 电源轨
      和充电配置、8 MB PSRAM、MCLK GPIO42、LCD reset GPIO39、触摸 reset GPIO40、可选
      TCA9554 探测、ES7210 主麦路由、16 kHz 全双工音频、PA 爆音/噪声、CO5300
      gap/颜色/对齐/亮度、CST9217 方向，以及 mybot 停止期间触摸和 Boot 配网。
- [ ] 对 Waveshare AMOLED 1.75C，仅使用 C profile；验证 USB 与电池启动、AXP2101 电源轨、
      8 MB PSRAM、MCLK GPIO16、LCD reset GPIO1、触摸 reset GPIO2、不探测 TCA9554、主麦
      路由、全双工音频、PA 噪声、显示/触摸与配网。扩展安全的 16 MB 分区布局前，必须先确认
      启动日志检测到的 Flash 容量。
- [ ] 对两个 Waveshare AMOLED 版本分别尝试配置另一版本的 profile 做负向测试，并确认发布
      流程绝不会交叉烧录两个固件制品。
- [ ] 对 SenseCAP Watcher，首次烧录前备份并校验 200 KiB `nvsfactory` 区域；确认正常烧录不
      改变该区域，且发布流程不得包含 `erase-flash`。
- [ ] 对 M5Stack StickS3，验证 USB 与电池启动、M5PM1 G2/G3 时序、扬声器爆音/噪声、
      16 kHz 采集 slot、ST7789P3 偏移与颜色，以及 mybot 停止期间 GPIO11 长按配网。

## 发布

- [ ] 创建与 `PROJECT_VER` 一致的带注释 `v<version>` tag。
- [ ] 仅在第三方授权审查后附加源码与固件制品。
- [ ] 包含 `LICENSE`、`THIRD_PARTY_NOTICES.md`、changelog、分区布局与校验和。
- [ ] 列出已知限制，并区分编译验证与真实硬件验证。
