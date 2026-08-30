# Contributing

## Workflow

1. Discuss large API, platform, dependency, protocol, partition or security changes in an issue.
2. Keep ESP-IDF and board-specific behavior in `components/mybot_platform`; do not add it to the
   vendored mybot core.
3. Never commit credentials, device tokens, Wi-Fi passwords, customer data or unapproved SDK builds.
4. Add an SPDX header to self-maintained C/C++ files and format touched files with `.clang-format`.
5. Document updates to vendored code in `VENDORED_SOURCES.md`.

Initialize the fixed toolchain and build before opening a pull request:

```sh
get_idf
test "$(idf.py --version)" = "ESP-IDF v5.5.2"
idf.py fullclean
idf.py build
idf.py size
git diff --check
```

Format self-maintained sources only:

```sh
find main components/mybot_platform -type f \
  \( -name '*.c' -o -name '*.h' -o -name '*.cc' -o -name '*.cpp' \) \
  -exec clang-format -i {} +
```

Platform pull requests must describe the problem, design, compatibility impact, tests, hardware,
network path and anything that still requires physical-device validation.

## Commit messages

Use Conventional Commits:

```text
<type>[optional scope][!]: <subject>

<optional body>

<optional footer>
```

Allowed types are `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`,
`chore` and `revert`. Keep the subject under 72 characters, use imperative mood, and add `!` plus a
`BREAKING CHANGE:` footer for incompatible changes.

Install the repository hook once per clone:

```sh
./scripts/setup-githooks.sh
```
