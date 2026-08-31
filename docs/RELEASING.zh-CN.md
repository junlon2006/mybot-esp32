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

- [ ] 两块板、两种语言及 20/40/60 ms 音频包长的 CI 全部通过。
- [ ] 两个 OTA 分区保留足够的回滚空间。
- [ ] 格式、SPDX、空白与提交信息检查通过。
- [ ] 在每个发布板卡上验证配网、重连、配对、HTTPS、双向音频、声纹状态、挂断、重复启停
      与重启持久化。
- [ ] 负向测试无效 CA、hostname 不匹配、TLS 超时、NVS 值缺失与 Wi-Fi 丢失。
- [ ] 确认日志与发布归档不包含任何凭据。
- [ ] 确认发布配置采用预期的 NVS/Flash encryption 与 Secure Boot 策略。

## 发布

- [ ] 创建与 `PROJECT_VER` 一致的带注释 `v<version>` tag。
- [ ] 仅在第三方授权审查后附加源码与固件制品。
- [ ] 包含 `LICENSE`、`THIRD_PARTY_NOTICES.md`、changelog、分区布局与校验和。
- [ ] 列出已知限制，并区分编译验证与真实硬件验证。
