#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
test_dir="$repo_root/tests/mcp_trace"
test_build_dir=$(mktemp -d "${TMPDIR:-/tmp}/mybot-mcp-trace.XXXXXX")
trap 'rm -r -- "$test_build_dir"' EXIT HUP INT TERM

set --
if [ "${SANITIZE:-0}" = 1 ]; then
    set -- -fsanitize=address,undefined -fno-omit-frame-pointer
fi

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror "$@" -Dvprintf=mock_vprintf \
    -Dvsnprintf=mock_trace_vsnprintf \
    -I"$test_dir/mocks" \
    -I"$repo_root/components/mybot_platform/include" \
    -c \
    "$repo_root/components/mybot_platform/src/services/diagnostics/mcp_trace.c" \
    -o "$test_build_dir/mcp_trace.o"

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror "$@" \
    -I"$test_dir/mocks" \
    -I"$repo_root/components/mybot_platform/include" \
    "$test_dir/test_mcp_trace.c" "$test_build_dir/mcp_trace.o" \
    -o "$test_build_dir/test_mcp_trace"

for initial_level in 0 3 5 6 7; do
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
        "$test_build_dir/test_mcp_trace" "$initial_level"
done
