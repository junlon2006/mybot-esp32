# Contributing

Contributions are welcome. Firmware versions and public behavior follow Semantic Versioning.

> [English](CONTRIBUTING.md) | [简体中文](CONTRIBUTING.zh-CN.md)

## Workflow

1. Discuss large platform, dependency, protocol, partition, or security changes in an issue first.
2. Keep application startup and MyBot lifecycle in `main/`, and board drivers and platform services
   in `components/mybot_platform`. The read-only MyBot snapshot is
   `components/mybot_stack/mybot_sdk/mybot`; platform code uses only its public `include/mybot/`
   headers. AOSL and Agora RTSA live beside it in `components/mybot_stack`, with independent
   component names and licenses.
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
  -DSDKCONFIG=build/contribution/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;ci/ptime60.defaults" build
idf.py -B build/contribution size
git diff --check
```

Use a separate build directory and sdkconfig for each board or configuration variant. The
[CI workflow](.github/workflows/ci.yml) defines 24 firmware builds: ten boards in both languages,
two additional CoreS3 video builds, one CoreS3 light-theme build, and one CoreS3 video build with
conversation animations disabled. All use the bundled RTSA package's 60 ms audio frames; 20 ms
and 40 ms configurations are rejected until a matching RTSA package is supplied.

LVGL is the only renderer on display boards; AtomEchoS3R and ReSpeaker Flex are headless. Do not add an opt-in
LVGL preset or a legacy-renderer fallback. See [board porting](docs/BOARD_PORTING.md) and
[platform UI](docs/PLATFORM_UI.md) for the current boundaries.

Format project-maintained sources:

```sh
find main components/mybot_platform -type f \
  \( -name '*.c' -o -name '*.h' -o -name '*.cc' -o -name '*.cpp' \) \
  -exec clang-format -i {} +
```

Pull requests must describe the problem, implementation, compatibility impact, validation, target
hardware, and remaining real-device checks. Distinguish firmware compilation, host-side tests,
and tests on a physical device; CI builds do not validate panel colors, audio quality, or power
sequencing. Dependency updates must identify the exact package and confirm that redistribution
terms and bundled notices remain valid.

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
