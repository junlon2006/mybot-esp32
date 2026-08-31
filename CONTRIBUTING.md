# Contributing

Contributions are welcome. Firmware versions and public behavior follow Semantic Versioning.

> [English](CONTRIBUTING.md) | [简体中文](CONTRIBUTING.zh-CN.md)

## Workflow

1. Discuss large platform, dependency, protocol, partition, or security changes in an issue first.
2. Keep ESP-IDF and board-specific behavior in `components/mybot_platform`; do not patch the
   vendored mybot SDK.
3. Never commit credentials, device tokens, Wi-Fi passwords, customer data, private endpoints, or
   unapproved SDK builds.
4. Add an SPDX header to project-maintained C/C++ files and format them with `.clang-format`.
5. Update user documentation and `CHANGELOG.md` when behavior changes.
6. Record dependency revisions in `VENDORED_SOURCES.md` and retain all required license notices.

Activate ESP-IDF v5.5.2 and build before opening a pull request:

```sh
. /path/to/esp-idf/export.sh
test "$(idf.py --version)" = "ESP-IDF v5.5.2"
idf.py -B build/contribution \
  -DMYBOT_BOARD=zhengchen-1.54tft-ml307 \
  -DSDKCONFIG=build/contribution/sdkconfig build
idf.py -B build/contribution size
git diff --check
```

Format project-maintained sources:

```sh
find main components/mybot_platform -type f \
  \( -name '*.c' -o -name '*.h' -o -name '*.cc' -o -name '*.cpp' \) \
  -exec clang-format -i {} +
```

Pull requests must describe the problem, implementation, compatibility impact, validation, target
hardware, and remaining real-device checks. Dependency updates must identify the exact package and
confirm that redistribution terms and bundled notices remain valid.

By submitting a contribution, you agree that it is licensed under the repository `LICENSE` unless
the file explicitly carries another compatible license.

## Commit Messages

Use [Conventional Commits](https://www.conventionalcommits.org/):

```text
<type>[optional scope][!]: <subject>

<optional body>

<optional footer>
```

Allowed types are `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`,
`chore`, and `revert`. Keep the subject under 72 characters, use imperative mood, and add `!` plus
a `BREAKING CHANGE:` footer for incompatible changes.

Install the repository hook once per clone:

```sh
./scripts/setup-githooks.sh
```
