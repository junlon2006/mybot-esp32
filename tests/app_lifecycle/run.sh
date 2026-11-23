#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
test_dir="$repo_root/tests/app_lifecycle"
test_build_dir=$(mktemp -d)
trap 'rm -r -- "$test_build_dir"' EXIT HUP INT TERM

# Use a shared host shim for the ESP-IDF headers included by app_main.c.
mkdir -p "$test_build_dir/include/freertos" "$test_build_dir/include/mybot" \
    "$test_build_dir/include/psa"
for header in esp_app_desc.h esp_event.h esp_idf_version.h esp_log.h esp_mac.h esp_netif.h \
    esp_netif_sntp.h esp_psram.h esp_timer.h freertos/FreeRTOS.h freertos/task.h \
    mybot/mybot_version.h nvs_flash.h psa/crypto.h; do
    printf '#include "host_idf.h"\n' > "$test_build_dir/include/$header"
done

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -DCONFIG_MYBOT_SERVER_BASE='"https://example.invalid"' -DMYBOT_CORES3_PIXEL_UI=1 \
    -I"$test_build_dir/include" -I"$test_dir" \
    -I"$repo_root/components/mybot_platform/include" \
    -I"$repo_root/components/mybot_stack/mybot_sdk/mybot/include" \
    -I"$repo_root/components/mybot_stack/agora_rtc/include" \
    "$repo_root/main/app_main.c" "$test_dir/test_app_lifecycle.c" -o "$test_build_dir/test_app"
python3 -c 'import subprocess,sys; subprocess.run(sys.argv[1:], check=True, timeout=20)' \
    "$test_build_dir/test_app"
