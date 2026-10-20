<!-- SPDX-License-Identifier: Apache-2.0 -->

# MCP trace host tests

Run from the repository root:

```sh
sh tests/mcp_trace/run.sh
SANITIZE=1 CC=clang sh tests/mcp_trace/run.sh
```

The tests compile the real platform trace bridge against public AOSL and ESP-IDF log/heap mocks.
Five fresh processes exercise startup at emergency, error, notice, info and debug levels.
They cover idempotent initialization, normal-log level preservation, RTM severity mapping,
literal percent signs in payloads, newline trimming, bounded truncation, null formats and
preservation of the caller's `va_list`. Heap checks require a 2048-byte PSRAM/8-bit allocation only
for RTM messages, verify release after success/truncation/format errors, and inject allocation
failure followed by recovery. A deterministic nested log call keeps two buffers live and verifies
that each message remains intact. Formatting-error injection applies only to the production
translation unit. The sanitizer run enables ASan and UBSan and disables LeakSanitizer; the heap
mock separately checks allocation/free balance and rejects invalid frees.

These are host checks of logging behavior. They do not establish RTM delivery or hardware volume
behavior on a device.
