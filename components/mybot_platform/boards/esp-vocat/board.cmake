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
    CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    CONFIG_ESP_CONSOLE_SECONDARY_NONE
    CONFIG_CODEC_ES8311_SUPPORT
    CONFIG_CODEC_ES7210_SUPPORT
    CONFIG_ESP_LCD_TOUCH_CST816S_DISABLE_READ_ID
)
set(MYBOT_BOARD_FORBIDDEN_CONFIGS
    CONFIG_ESP_CONSOLE_UART_DEFAULT
    CONFIG_ESP_CONSOLE_UART
    CONFIG_CODEC_I2C_BACKWARD_COMPATIBLE
)
set(MYBOT_BOARD_PARTITION_TABLE "partitions/v2/16m.csv")

get_filename_component(MYBOT_PLATFORM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(MYBOT_BOARD_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/board.c"
    "${CMAKE_CURRENT_LIST_DIR}/vocat_hardware.c"
    "${CMAKE_CURRENT_LIST_DIR}/vocat_input.c"
    "${MYBOT_PLATFORM_ROOT}/src/drivers/audio/vocat_codec_audio.c"
    "${MYBOT_PLATFORM_ROOT}/src/drivers/display/vocat_st77916_lcd.c"
)
if(CONFIG_MYBOT_LVGL_UI)
    list(APPEND MYBOT_BOARD_SOURCES
        "${MYBOT_PLATFORM_ROOT}/src/drivers/display/dynamic_lvgl_lcd.cc"
        "${MYBOT_PLATFORM_ROOT}/src/drivers/display/vocat_lvgl_panel.c"
        "${MYBOT_PLATFORM_ROOT}/src/drivers/display/cores3_lvgl_view.cc"
        "${MYBOT_PLATFORM_ROOT}/src/drivers/display/cores3_ui_assets.c"
        "${MYBOT_PLATFORM_ROOT}/src/drivers/display/cores3_lvgl_font.c")
endif()
set(MYBOT_BOARD_REQUIRES
    button
    esp_codec_dev
    esp_driver_gpio
    esp_driver_i2c
    esp_driver_i2s
    esp_driver_spi
    esp_lcd
    esp_lcd_st77916
    esp_lcd_touch
    esp_lcd_touch_cst816s
    esp_lvgl_port
    lvgl
    nvs_flash
)
if(CONFIG_MYBOT_LVGL_UI)
    set_property(SOURCE
        "${MYBOT_PLATFORM_ROOT}/src/drivers/display/dynamic_lvgl_lcd.cc"
        APPEND PROPERTY COMPILE_DEFINITIONS MYBOT_LVGL_PANEL_VOCAT)
endif()
