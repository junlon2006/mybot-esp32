# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Project Contributors

set(MYBOT_BOARD_TARGET "esp32s3")
set(MYBOT_BOARD_SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
set(MYBOT_BOARD_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(MYBOT_BOARD_REQUIRED_CONFIGS
    CONFIG_PARTITION_TABLE_CUSTOM
    CONFIG_ESPTOOLPY_FLASHSIZE_32MB
    CONFIG_ESPTOOLPY_FLASHMODE_QIO
    CONFIG_IDF_EXPERIMENTAL_FEATURES
    CONFIG_BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH
    CONFIG_SPIRAM
    CONFIG_SPIRAM_MODE_OCT
    CONFIG_SPIRAM_SPEED_80M
    CONFIG_CODEC_ES8311_SUPPORT
    CONFIG_CODEC_ES7243E_SUPPORT
)
set(MYBOT_BOARD_FORBIDDEN_CONFIGS
    CONFIG_ESPTOOLPY_FLASH_MODE_AUTO_DETECT
    CONFIG_CODEC_I2C_BACKWARD_COMPATIBLE
    CONFIG_IO_EXPANDER_ENABLE_GPIO_API_WRAPPER
)
set(MYBOT_BOARD_PARTITION_TABLE "partitions/v2/32m-sensecap.csv")

get_filename_component(MYBOT_PLATFORM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(MYBOT_BOARD_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/board.c"
    "${CMAKE_CURRENT_LIST_DIR}/sensecap_hardware.c"
    "${CMAKE_CURRENT_LIST_DIR}/sensecap_input.c"
    "${MYBOT_PLATFORM_ROOT}/src/drivers/audio/sensecap_codec_audio.c"
    "${MYBOT_PLATFORM_ROOT}/src/drivers/display/spd2010_lcd.c"
)
set(MYBOT_BOARD_REQUIRES
    esp_codec_dev
    esp_driver_gpio
    esp_driver_i2c
    esp_driver_i2s
    esp_driver_ledc
    esp_driver_spi
    esp_io_expander_tca95xx_16bit
    esp_lcd
    esp_lcd_spd2010
    esp_timer
    knob
)
