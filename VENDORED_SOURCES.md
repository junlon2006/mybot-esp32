# Vendored source baselines

本工程将构建所需的依赖源码固定在仓库中，避免 ESP-IDF Component Registry 版本漂移。

| 路径 | 来源 | 固定版本 |
| --- | --- | --- |
| `third_party/mybot` | `junlon2006/mybot` | commit `38153e78acc22452c8f7afcddbcd522274dad2d6` |
| `components/aosl` | `junlon2006/xiaozhi-esp32` | commit `ad2f7da4c9ba77294a5abb48f29e895fe486ed0e` |
| `components/agora_rtc` | `junlon2006/xiaozhi-esp32` | Agora RTSA 1.10.0, build 1154652 |
| `components/esp-wifi-connect` | `78/esp-wifi-connect` | 3.2.2, commit `c24b97c194e6b4a1d7be0237b3c28980661cac1e` |
| `components/button` | `espressif/button` | 4.2.0, commit `5f9cb98ae4d0e8153c4b4d1accf471214e5b6fe8` |
| `components/cmake_utilities` | Espressif cmake utilities | 0.5.0 |

Local changes relative to those baselines:

- `components/aosl/include/api/aosl.h` and `components/aosl/kernel/mpq.c` carry BK7258 commit
  `9956b939fe70dcc741b028e8b53a66aaab2880a9` for reference-counted AOSL ownership.
- `third_party/mybot/src/rtc/mybot_agora_rtc.c` uses C99 `PRIu32` format macros so `uint32_t`
  logging is valid for the ESP32-S3 ABI. The HTTP client omits plaintext-only socket helpers from
  HTTPS-only builds.
- Vendored component manifests are omitted. Dependencies are local or supplied by ESP-IDF v5.5.2.
- The button wrapper defines its pinned `4.2.0` version macros directly because the upstream
  package helper normally reads those values from the omitted Registry manifest.

When updating a dependency, update this file, `THIRD_PARTY_NOTICES.md`, build with a clean sdkconfig,
and report the target-board validation performed.
