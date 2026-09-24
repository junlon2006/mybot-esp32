#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
test_dir="$repo_root/tests/atom_echos3r"
test_build_dir=$(mktemp -d)
trap 'rm -r -- "$test_build_dir"' EXIT HUP INT TERM

"${CC:-cc}" -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pthread \
    -I"$test_dir/mocks" \
    -I"$repo_root/components/mybot_platform/boards/atom-echos3r" \
    "$test_dir/test_atom_echos3r.c" \
    "$repo_root/components/mybot_platform/boards/atom-echos3r/atom_echos3r_hardware.c" \
    "$repo_root/components/mybot_platform/boards/atom-echos3r/atom_echos3r_input.c" \
    -o "$test_build_dir/test_atom_echos3r"

if command -v timeout >/dev/null 2>&1; then
    timeout 20 "$test_build_dir/test_atom_echos3r"
else
    "$test_build_dir/test_atom_echos3r"
fi
