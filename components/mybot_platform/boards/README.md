# Board documentation / 板卡说明

All 13 supported board profiles have a README in their own directory. Reference documents are
copied verbatim, including their original build instructions and feature descriptions. For this
firmware, use the [English build guide](../../../README.md#build) or
[中文构建说明](../../../README.zh-CN.md#构建); the active profile is defined by each `board.cmake`
and `board_config.h`.

已有的板卡说明原样复制自参考工程，保留原文内容、行尾和格式，其中的菜单、脚本、固件名称
及功能描述属于原工程。本工程使用 `MYBOT_BOARD` 和 `scripts/build_all.py` 构建，实际引脚与
支持能力以各目录的 `board_config.h`、`board.cmake` 及本工程文档为准。

| Board profile | 说明 | 来源 |
| --- | --- | --- |
| `atom-echos3r` | [README](atom-echos3r/README.md) | 参考原文 |
| `bread-compact-wifi-lcd` | [README](bread-compact-wifi-lcd/README.md) | 参考工程无对应 README，按当前 profile 补充 |
| `esp-vocat` | [README](esp-vocat/README.md) | 参考原文 |
| `esp32-s3-touch-amoled-1.75` | [README](esp32-s3-touch-amoled-1.75/README.md) | 参考原文 |
| `esp32-s3-touch-amoled-1.75c` | [README](esp32-s3-touch-amoled-1.75c/README.md) | 与 1.75 共用参考原文，与上游板型映射一致 |
| `esp32-s3-touch-amoled-2.16` | [README](esp32-s3-touch-amoled-2.16/README.md) | 参考原文 |
| `m5stack-core-s3` | [README](m5stack-core-s3/README.md) | 参考原文 |
| `m5stack-stick-s3` | [README](m5stack-stick-s3/README.md) | 参考原文 |
| `respeaker-flex-xvf3800-circular4-xiao` | [README](respeaker-flex-xvf3800-circular4-xiao/README.md) | 参考工程无对应 README，按当前 profile 补充 |
| `sensecap-watcher` | [中文](sensecap-watcher/README.md) / [English](sensecap-watcher/README_en.md) | 参考原文 |
| `xingzhi-cube-1.54tft-wifi` | [README](xingzhi-cube-1.54tft-wifi/README.md) | 参考工程无对应 README，按当前 profile 补充 |
| `zhengchen-1.54tft-ml307` | [README](zhengchen-1.54tft-ml307/README.md) | 参考原文 |
| `zhengchen-1.54tft-wifi` | [README](zhengchen-1.54tft-wifi/README.md) | 参考原文 |

Source mappings and the pinned revision are recorded in
[VENDORED_SOURCES.md](../../../VENDORED_SOURCES.md#board-reference-documents), with license terms in
[THIRD_PARTY_NOTICES.md](../../../THIRD_PARTY_NOTICES.md).
