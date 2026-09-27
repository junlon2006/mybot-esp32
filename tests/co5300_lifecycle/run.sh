#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
test_dir="$repo_root/tests/co5300_lifecycle"
test_build_dir=$(mktemp -d)
trap 'rm -r -- "$test_build_dir"' EXIT HUP INT TERM

if [ "$#" -eq 0 ]; then
    set -- esp32-s3-touch-amoled-1.75 esp32-s3-touch-amoled-1.75c esp32-s3-touch-amoled-2.16
fi
for board_id do
    panel_source="$repo_root/components/mybot_platform/src/drivers/display/panels/co5300/co5300_panel.c"
    if [ "$board_id" = esp32-s3-touch-amoled-2.16 ]; then
        panel_source="$repo_root/components/mybot_platform/src/drivers/display/panels/co5300/co5300_480_panel.c"
    fi
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -I"$test_dir/mocks" -I"$repo_root/tests/st7789_lifecycle/mocks" \
        -I"$repo_root/components/mybot_platform/boards/$board_id" \
        -I"$repo_root/components/mybot_platform/src/internal" \
        "$test_dir/test_co5300_lifecycle.c" \
        "$panel_source" \
        -o "$test_build_dir/test_co5300"
    if command -v timeout >/dev/null 2>&1; then
        timeout 20 "$test_build_dir/test_co5300"
    else
        "$test_build_dir/test_co5300"
    fi
done

board_dir="$repo_root/components/mybot_platform/boards/esp32-s3-touch-amoled-2.16"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -I"$repo_root/tests/st7789_lifecycle/mocks" -I"$board_dir" \
    -I"$repo_root/components/mybot_platform/boards/esp32-s3-touch-amoled-1.75-common" \
    -I"$repo_root/components/mybot_platform/include" \
    -I"$repo_root/components/mybot_platform/src/internal" \
    -I"$repo_root/components/mybot_stack/mybot_sdk/mybot/include" \
    -c "$board_dir/board.c" -o "$test_build_dir/board.o"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread \
    -I"$repo_root/tests/st7789_lifecycle/mocks" -I"$board_dir" \
    -I"$repo_root/components/mybot_platform/boards/esp32-s3-touch-amoled-1.75-common" \
    -I"$repo_root/components/mybot_platform/include" \
    -I"$repo_root/components/mybot_platform/src/internal" \
    -I"$repo_root/components/mybot_stack/mybot_sdk/mybot/include" \
    "$test_dir/test_amoled216_board.cc" "$test_build_dir/board.o" \
    -o "$test_build_dir/test_amoled216_board"
for scenario in hardware_init lcd_init lcd_render input_start input_stop_repeat hardware_deinit ok; do
    "$test_build_dir/test_amoled216_board" "$scenario"
done
