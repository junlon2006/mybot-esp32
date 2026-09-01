# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Project Contributors

set(MYBOT_BOARD_TARGET "esp32s3")
set(MYBOT_BOARD_SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
set(MYBOT_BOARD_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(MYBOT_BOARD_REQUIRED_CONFIGS
    CONFIG_PARTITION_TABLE_CUSTOM
    CONFIG_ESPTOOLPY_FLASHSIZE_16MB
    CONFIG_ESPTOOLPY_FLASHMODE_QIO
    CONFIG_SPIRAM
    CONFIG_SPIRAM_MODE_OCT
    CONFIG_SPIRAM_SPEED_80M
    CONFIG_CODEC_ES8311_SUPPORT
    CONFIG_CODEC_ES7210_SUPPORT
)
set(MYBOT_BOARD_FORBIDDEN_CONFIGS
    CONFIG_CODEC_I2C_BACKWARD_COMPATIBLE
)
set(MYBOT_BOARD_PARTITION_TABLE "partitions/v2/16m.csv")

get_filename_component(MYBOT_PLATFORM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(MYBOT_BOARD_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/board.c"
    "${CMAKE_CURRENT_LIST_DIR}/amoled175_hardware.c"
    "${CMAKE_CURRENT_LIST_DIR}/amoled175_input.c"
    "${MYBOT_PLATFORM_ROOT}/src/drivers/audio/amoled175_codec_audio.c"
    "${MYBOT_PLATFORM_ROOT}/src/drivers/display/amoled175_co5300_lcd.c"
)
set(MYBOT_BOARD_REQUIRES
    button
    esp_codec_dev
    esp_driver_gpio
    esp_driver_i2c
    esp_driver_i2s
    esp_driver_spi
    esp_lcd
    esp_lcd_co5300
    esp_lcd_touch
    esp_lcd_touch_cst9217
    nvs_flash
)
