# mybot-esp32 agent guide

This file applies to the whole repository. Follow it together with the user's request. User
instructions have priority when they conflict with this guide. Keep changes small, preserve
unrelated worktree changes, and report validation results with the final handoff.

## Project boundary

`mybot-esp32` is an ESP-IDF 5.5.2 firmware project for ESP32-S3. Its platform code lives in
`components/mybot_platform`; the runtime inputs are grouped under `components/mybot_stack`:

```text
components/mybot_stack/
  mybot_sdk/CMakeLists.txt       ESP-IDF wrapper
  mybot_sdk/mybot/               immutable MyBot SDK snapshot
  aosl/                          immutable AOSL component
  agora_rtc/                     immutable Agora RTSA component
components/mybot_platform/
  boards/<board-id>/             board profile and hardware lifecycle
  src/services/                  Wi-Fi, storage, announcements, audio, diagnostics
  src/drivers/                   audio, display, input, video
  src/internal/                  private platform headers
main/                             application lifecycle and project Kconfig
scripts/build_all.py              multi-board firmware builder
```

The parent `components/mybot_stack` directory is only a bundle directory. The three child
directories remain independent ESP-IDF components and are registered by the root `CMakeLists.txt`.

## Allowed and forbidden edits

An agent may edit:

- `main/`, `components/mybot_platform/`, board profile files, platform drivers and services;
- `components/mybot_platform/boards/boards.cmake`, root/project Kconfig, CI, scripts and Markdown
  documentation;
- local build metadata such as `CMakeLists.txt` when it is required to register a component or
  board.

An agent must not edit, patch, reformat, or add files under:

- `components/mybot_stack/mybot_sdk/mybot/`;
- `components/mybot_stack/aosl/`;
- `components/mybot_stack/agora_rtc/`;
- any external reference repository.

Platform code may include MyBot only through public headers under
`components/mybot_stack/mybot_sdk/mybot/include/mybot/`. Never include SDK private headers,
private sources, or undeclared SDK symbols. Do not add ESP-IDF, FreeRTOS, GPIO, I2S, LCD, NVS, or
socket details to the SDK snapshot.

Do not hand-edit generated `build/`, `sdkconfig*`, `releases/`, `managed_components/`, compiler
output, or dependency caches. Change defaults, Kconfig, CMake or scripts and regenerate them.
`scripts/build_all.py` intentionally cleans `build/` and `releases/` before a normal batch; use
`--no-clean` only when an incremental build is required.

Do not commit credentials, device identifiers, Wi-Fi passwords, private endpoints, generated
firmware, or unapproved third-party binaries. Do not run `git commit`, push, reset, checkout, tag,
or other history-changing commands unless the user explicitly requests that action.

## Adding a new board

Complete these steps in order. Stop and report a blocking hardware/API assumption before inventing
a new SDK interface.

1. Inspect the hardware reference and existing board drivers. Record target SoC, Flash, PSRAM mode,
   partition layout, console, power rails, codec clocks, display controller, touch/input devices,
   and camera capability. Keep the product documentation independent of external application
   projects; use references only for hardware facts and required license notices.

2. Choose a stable lower-case board ID. Create
   `components/mybot_platform/boards/<board-id>/` with at least:

   - `board.cmake`: target, board defaults, include directories, required/forbidden Kconfig symbols,
     partition table, source files and component requirements;
   - `board.c`: process-lifetime hardware preparation, platform descriptor registration, network
     prerequisite, provisioning trigger and shutdown;
   - `board_config.h`: pins, dimensions, clocks, offsets and board constants;
   - `sdkconfig.defaults`: Flash, PSRAM, console, codec and partition defaults.

3. Add the ID and profile mapping to `components/mybot_platform/boards/boards.cmake`. The root
   `MYBOT_BOARD` CMake value is the only board selector. A board profile is selected at build time;
   runtime hardware-revision detection is allowed only when compatible revisions share the same
   target, partition and component contract.

4. Keep lifecycle ownership in the board or reusable platform driver. `prepare()` must initialize
   hardware before registering the platform. `ensure_network()` returns only after usable IP
   connectivity; `provision_wifi()` returns after provisioning reconnects the station;
   `shutdown_network()` tears down the network. `main/app_main.c` starts MyBot only after the
   network prerequisite and stops it before button-triggered provisioning.

5. Preserve the SDK audio contract: 16 kHz, mono, signed 16-bit PCM and frame counts, never byte
   counts. Keep capture/playback independent and reference-count shared I2S/codec resources.
   Playback prompts must drain their buffer; stop and destroy must be bounded and retry-safe.

6. For a display, put controller reset, bus, power and backlight lifecycle in
   `src/drivers/display/panels/`. Connect it through `display.cmake` and a LVGL adapter. All
   display boards use the shared LVGL view automatically; never add a legacy renderer or a manual
   LVGL enable switch. For a headless board, select `Kconfig.headless` in
   `components/mybot_platform/CMakeLists.txt` and do not compile LVGL sources.

7. For video, add a complete `mybot_video_ops_t` source only when the board has a validated camera
   path. Add the capability to the board CMake profile and ensure unsupported boards reject
   `CONFIG_MYBOT_ENABLE_VIDEO`. Capture initialization must not start workers; start/stop must own
   worker and callback lifetimes, and destroy must not free resources while a frame callback is in
   flight. Keep source capture rate, DMA buffering and upload rate separate in logs and docs.

8. The automatic builder derives display capability from `mybot_display_add_*` and video capability
   from the CoreS3 camera source name. If a new video board uses another source layout, update
   `scripts/build_all.py` capability detection and add an explicit dry-run test. The default builder
   command is:

   ```sh
   source /path/to/esp-idf/export.sh
   python3 scripts/build_all.py --board <board-id> --language both
   ```

   The script builds Chinese and English variants, appends `ci/ptime60.defaults`, automatically
   enables video on capable boards, runs `reconfigure build size merge-bin`, and writes merged
   images to `releases/`. Use `--video off` for a no-video comparison and `--no-clean` only for
   incremental work.

9. Add both languages to `.github/workflows/ci.yml`. Add video variants for a video-capable board
   and theme/static variants only where they exercise supported options. Keep the bundled RTSA
   cadence at 60 ms unless a matching RTSA package is deliberately supplied and documented.

10. Update English and Chinese board/UI/video documentation, `CHANGELOG.md`,
    `VENDORED_SOURCES.md` and `THIRD_PARTY_NOTICES.md` when sources, dependencies, licenses or
    hardware references change. Document build validation separately from real-device validation.

## Validation gate

Before declaring a board ready:

```sh
source /path/to/esp-idf/export.sh
test "$(idf.py --version)" = "ESP-IDF v5.5.2"
python3 scripts/build_all.py --board <board-id> --language both
git diff --check
```

Also run clang-format and SPDX checks for `main` and `components/mybot_platform`, inspect both
generated sdkconfigs, and confirm:

- display boards have `CONFIG_MYBOT_LVGL_UI=y`; headless boards omit the LVGL runtime;
- video is enabled only on a board with a validated `mybot_video_ops_t` implementation;
- Flash, PSRAM, console, codec and partition checks pass for the selected profile;
- provisioning, Wi-Fi reconnect, HTTPS, pairing, bidirectional audio, volume persistence,
  input, display colors/orientation, repeated MyBot start/stop, and failure cleanup are tested;
- video tests include camera format, frame cadence, server reception, stop/restart and concurrent
  audio; resource logs record per-core CPU, internal heap and PSRAM;
- hardware results are labeled as real-device results. A host test or successful build does not
  establish audio quality, panel behavior, power sequencing or camera inference.

For lifecycle or concurrency changes, include a host fault-injection test covering initialization
failure, partial cleanup, timeout, repeated start/stop, callback ordering and resource release.
Do not claim a new board is hardware-validated until the relevant physical tests have actually run.
