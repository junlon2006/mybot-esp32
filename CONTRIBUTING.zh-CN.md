# 参与贡献

欢迎参与贡献。固件版本与公开行为遵循语义化版本。

> [English](CONTRIBUTING.md) | [简体中文](CONTRIBUTING.zh-CN.md)

## 工作流程

1. 涉及平台、依赖、协议、分区或安全的大型改动，请先在 issue 中讨论。
2. 应用启动与 MyBot 启停流程放在 `main/`，板级驱动和平台服务放在 `components/mybot_platform`。
   MyBot 只读快照位于 `components/mybot_stack/mybot_sdk/mybot`，平台代码只能使用其公开的
   `include/mybot/` 头文件。AOSL 与 Agora RTSA 也位于 `components/mybot_stack`，各自保持独立的
   组件名称和许可证。
3. 绝不提交凭据、设备 token、Wi-Fi 密码、客户数据、私有服务地址或未经批准的 SDK 构建。
4. 工程自维护的 C/C++ 文件必须带 SPDX 头，并使用 `.clang-format` 格式化。
5. 用户可见行为变化时同步更新文档与 `CHANGELOG.md`。
6. 在 `VENDORED_SOURCES.md` 记录依赖版本，并保留所有必要的许可证声明。

提交 pull request 前，激活 ESP-IDF v5.5.2 并构建：

```sh
. /path/to/esp-idf/export.sh
test "$(idf.py --version)" = "ESP-IDF v5.5.2"
idf.py -B build/contribution \
  -DMYBOT_BOARD=zhengchen-1.54tft-ml307 \
  -DSDKCONFIG=build/contribution/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;ci/ptime60.defaults" build
idf.py -B build/contribution size
git diff --check
```

每个板型或配置变体使用独立构建目录和 sdkconfig。[CI 工作流](.github/workflows/ci.yml)
定义了 24 项固件构建：十个板型分别构建中英文版本，另加 CoreS3 两种语言的视频构建、
一个浅色主题构建，以及一个关闭会话动画的视频构建。全部使用随附 RTSA 支持的 60 ms
音频帧；在提供匹配的 RTSA 包之前，20 ms 和 40 ms 配置会被拒绝。

所有带屏板型只使用 LVGL，AtomEchoS3R 与 ReSpeaker Flex 保持无屏；不要新增启用 LVGL 的可选 preset 或旧
渲染器回退。职责边界见[板级移植](docs/BOARD_PORTING.zh-CN.md)和
[平台 UI](docs/PLATFORM_UI.zh-CN.md)。

格式化工程自维护源码：

```sh
find main components/mybot_platform -type f \
  \( -name '*.c' -o -name '*.h' -o -name '*.cc' -o -name '*.cpp' \) \
  -exec clang-format -i {} +
```

Pull request 必须说明问题、实现、兼容性影响、验证结果、目标硬件，以及仍待完成的真机检查。
分别记录固件编译、主机测试和真机测试；CI 构建不能验证屏幕颜色、音质或上电时序。依赖更新
必须标明准确包版本，并确认再分发条款与随附声明仍然有效。

提交贡献即表示你同意按仓库 `LICENSE` 发布该贡献，除非文件明确带有其他兼容许可证。

## 提交信息

使用 [Conventional Commits](https://www.conventionalcommits.org/)：

```text
<type>[optional scope][!]: <subject>

<optional body>

<optional footer>
```

允许的类型为 `feat`、`fix`、`docs`、`style`、`refactor`、`perf`、`test`、`build`、`ci`、
`chore` 与 `revert`。主题行不超过 72 个字符并使用祈使句；不兼容变更需添加 `!` 和
`BREAKING CHANGE:` footer。

每个克隆安装一次仓库 hook：

```sh
./scripts/setup-githooks.sh
```
