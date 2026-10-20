# Bread Compact Wi-Fi LCD

本说明根据本工程的 [board.cmake](board.cmake)、[board_config.h](board_config.h) 和
[sdkconfig.defaults](sdkconfig.defaults) 整理；参考工程没有该板型的独立 README。

## 硬件配置

| 项目 | 当前 profile |
| --- | --- |
| MCU | ESP32-S3，N16R8 配置 |
| Flash | 16 MB，QIO |
| PSRAM | 8 MB，Octal，80 MHz |
| 显示 | ST7789，240×320，SPI；GPIO42 PWM 背光 |
| 音频 | 麦克风与扬声器使用独立 I2S，16 kHz 单声道 |
| 输入 | GPIO0 Boot 按键，无独立音量键、无触摸 |
| 网络 | Wi-Fi |
| 分区 | `partitions/v2/16m.csv` |

麦克风 WS/BCLK/DIN 分别为 GPIO4/5/6，扬声器 DOUT/BCLK/WS 分别为 GPIO7/15/16。
显示引脚及方向配置见 `board_config.h`。当前 profile 没有摄像头支持。

## 构建

在工程根目录激活 ESP-IDF v5.5.2 后执行：

```sh
python3 scripts/build_all.py --board bread-compact-wifi-lcd --language both
```

构建与烧录的完整说明见[项目文档](../../../../README.zh-CN.md#构建)。当前板型的
实机验证状态见[平台移植文档](../../../../docs/BOARD_PORTING.zh-CN.md)。
