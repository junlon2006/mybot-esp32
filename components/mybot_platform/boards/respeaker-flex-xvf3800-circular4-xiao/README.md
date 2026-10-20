# ReSpeaker Flex XVF3800 Circular-4 with XIAO ESP32S3

本说明根据本工程的 [board.cmake](board.cmake)、[board_config.h](board_config.h) 和
[sdkconfig.defaults](sdkconfig.defaults) 整理；参考工程没有该板型的独立 README。
硬件介绍见 [Seeed Studio ReSpeaker Flex 文档](https://wiki.seeedstudio.com/cn/respeaker_flex_introduction/)。

## 硬件配置

| 项目 | 当前 profile |
| --- | --- |
| MCU | XIAO ESP32S3 |
| Flash | 8 MB，QIO |
| PSRAM | 8 MB，Octal，80 MHz |
| 音频 | XVF3800 麦克风前端与 AIC3104 输出，SDK 使用 16 kHz 单声道 PCM |
| I2S | BCLK GPIO8、WS GPIO7、DOUT GPIO44、DIN GPIO43 |
| I2C | SDA GPIO5、SCL GPIO6，400 kHz；AIC3104 `0x18`、XVF3800 `0x2c` |
| 输入 | XIAO Boot GPIO0 与 XVF3800 板载按键 |
| 显示 | 无屏幕，不编译 LVGL |
| 控制台 | USB Serial/JTAG；GPIO43/44 已用于 I2S |
| 网络 | Wi-Fi |
| 分区 | `partitions/v2/8m.csv` |

XVF3800 提供硬件 AEC，当前 profile 关闭 Cloud AEC。当前 profile 没有摄像头支持。

## 构建

在工程根目录激活 ESP-IDF v5.5.2 后执行：

```sh
python3 scripts/build_all.py --board respeaker-flex-xvf3800-circular4-xiao --language both
```

构建与烧录的完整说明见[项目文档](../../../../README.zh-CN.md#构建)。当前板型的
实机验证状态见[平台移植文档](../../../../docs/BOARD_PORTING.zh-CN.md)。
