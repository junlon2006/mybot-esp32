#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
test_dir="$repo_root/tests/bread_compact_wifi_lcd"
board_dir="$repo_root/components/mybot_platform/boards/bread-compact-wifi-lcd"
test_build_dir=$(mktemp -d)
trap 'rm -r -- "$test_build_dir"' EXIT HUP INT TERM

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -I"$test_dir/mocks" -I"$repo_root/tests/st7789_lifecycle/mocks" -I"$board_dir" \
    -I"$repo_root/components/mybot_platform/src/internal" \
    -c "$repo_root/components/mybot_platform/src/drivers/display/panels/st7789/st7789_panel.c" \
    -o "$test_build_dir/panel.o"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread \
    -I"$test_dir/mocks" -I"$repo_root/tests/st7789_lifecycle/mocks" -I"$board_dir" \
    -I"$repo_root/components/mybot_platform/src/internal" \
    -I"$repo_root/components/mybot_stack/mybot_sdk/mybot/include" \
    "$repo_root/tests/st7789_lifecycle/test_st7789_lifecycle.cc" \
    "$repo_root/components/mybot_platform/src/drivers/display/adapters/lvgl/shared_lvgl_adapter.cc" \
    "$test_build_dir/panel.o" -o "$test_build_dir/test_bread_display"

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -I"$test_dir/mocks" -I"$repo_root/tests/st7789_lifecycle/mocks" -I"$board_dir" \
    -I"$repo_root/components/mybot_platform/include" \
    -I"$repo_root/components/mybot_platform/src/internal" \
    -I"$repo_root/components/mybot_stack/mybot_sdk/mybot/include" \
    -c "$board_dir/board.c" -o "$test_build_dir/board.o"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread \
    -I"$test_dir/mocks" -I"$repo_root/tests/st7789_lifecycle/mocks" -I"$board_dir" \
    -I"$repo_root/components/mybot_platform/include" \
    -I"$repo_root/components/mybot_platform/src/internal" \
    -I"$repo_root/components/mybot_stack/mybot_sdk/mybot/include" \
    "$test_dir/test_bread_board.cc" "$test_build_dir/board.o" \
    -o "$test_build_dir/test_bread_board"

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pthread \
    -I"$test_dir/mocks" -I"$repo_root/tests/st7789_lifecycle/mocks" -I"$board_dir" \
    -I"$repo_root/components/mybot_platform/src/internal" \
    -I"$repo_root/components/mybot_stack/mybot_sdk/mybot/include" \
    -c "$board_dir/bread_input.c" -o "$test_build_dir/input.o"
"${CXX:-c++}" -std=c++17 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pthread \
    -I"$test_dir/mocks" -I"$repo_root/tests/st7789_lifecycle/mocks" -I"$board_dir" \
    -I"$repo_root/components/mybot_platform/src/internal" \
    -I"$repo_root/components/mybot_stack/mybot_sdk/mybot/include" \
    "$test_dir/test_bread_input.cc" "$test_build_dir/input.o" \
    -o "$test_build_dir/test_bread_input"

run_host_test() {
    if command -v timeout >/dev/null 2>&1; then
        timeout 20 "$@"
    else
        "$@"
    fi
}

run_host_test "$test_build_dir/test_bread_display"
for scenario in ledc_timer ledc_channel ledc_duty ledc_update lcd_init lcd_render input_start ok; do
    run_host_test "$test_build_dir/test_bread_board" "$scenario"
done
for scenario in ctor partial_ctor register_click register_long ok; do
    run_host_test "$test_build_dir/test_bread_input" "$scenario"
done
