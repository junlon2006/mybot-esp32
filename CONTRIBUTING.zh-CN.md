# 参与贡献

欢迎参与贡献。固件版本与公开行为遵循语义化版本。

> [English](CONTRIBUTING.md) | [简体中文](CONTRIBUTING.zh-CN.md)

## 工作流程

1. 涉及平台、依赖、协议、分区或安全的大型改动，请先在 issue 中讨论。
2. ESP-IDF 与板级行为应放在 `components/mybot_platform`，不要补丁修改固定版本的 mybot SDK。
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
  -DSDKCONFIG=build/contribution/sdkconfig build
idf.py -B build/contribution size
git diff --check
```

格式化工程自维护源码：

```sh
find main components/mybot_platform -type f \
  \( -name '*.c' -o -name '*.h' -o -name '*.cc' -o -name '*.cpp' \) \
  -exec clang-format -i {} +
```

Pull request 必须说明问题、实现、兼容性影响、验证结果、目标硬件，以及仍待完成的真机检查。
依赖更新必须标明准确包版本，并确认再分发条款与随附声明仍然有效。

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
