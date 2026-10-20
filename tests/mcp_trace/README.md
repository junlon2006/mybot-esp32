<!-- SPDX-License-Identifier: Apache-2.0 -->

# MCP trace host tests

Run from the repository root:

```sh
sh tests/mcp_trace/run.sh
SANITIZE=1 CC=clang sh tests/mcp_trace/run.sh
```

The tests compile the real platform trace bridge against public AOSL and ESP-IDF log mocks.
Five fresh processes exercise startup at emergency, error, notice, info and debug levels.
They cover idempotent initialization, normal-log level preservation, RTM severity mapping,
literal percent signs in payloads, newline trimming, bounded truncation, null formats and
preservation of the caller's `va_list`. The sanitizer run enables ASan and UBSan and disables
LeakSanitizer.

These are host checks of logging behavior. They do not establish RTM delivery or hardware volume
behavior on a device.
