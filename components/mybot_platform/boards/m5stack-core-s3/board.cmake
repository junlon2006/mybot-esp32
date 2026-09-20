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
    CONFIG_SPIRAM_MODE_QUAD
    CONFIG_SPIRAM_SPEED_80M
    CONFIG_CODEC_ES7210_SUPPORT
    CONFIG_CODEC_AW88298_SUPPORT
)
set(MYBOT_BOARD_FORBIDDEN_CONFIGS
    CONFIG_CODEC_I2C_BACKWARD_COMPATIBLE
)
set(MYBOT_BOARD_PARTITION_TABLE "partitions/v2/16m.csv")

get_filename_component(MYBOT_PLATFORM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(MYBOT_BOARD_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/board.c"
    "${CMAKE_CURRENT_LIST_DIR}/cores3_hardware.c"
    "${MYBOT_PLATFORM_ROOT}/src/drivers/audio/cores3_codec_audio.c"
    "${MYBOT_PLATFORM_ROOT}/src/drivers/display/cores3_lcd_panel.c"
    "${MYBOT_PLATFORM_ROOT}/src/drivers/input/ft6336_touch.c"
)
set(MYBOT_BOARD_REQUIRES
    esp_codec_dev
    esp_lcd_ili9341
    esp_driver_gpio
    esp_driver_i2c
    esp_driver_i2s
    esp_driver_spi
    esp_lcd
    esp_timer
    nvs_flash
    esp_video
    esp_new_jpeg
    esp_lvgl_port
)

if(CONFIG_MYBOT_LVGL_UI)
    list(APPEND MYBOT_BOARD_SOURCES
        "${MYBOT_PLATFORM_ROOT}/src/drivers/display/cores3_lvgl_lcd.cc"
        "${MYBOT_PLATFORM_ROOT}/src/drivers/display/cores3_lvgl_view.cc"
        "${MYBOT_PLATFORM_ROOT}/src/drivers/display/cores3_ui_assets.c"
        "${MYBOT_PLATFORM_ROOT}/src/drivers/display/cores3_lvgl_font.c")
else()
    list(APPEND MYBOT_BOARD_SOURCES "${MYBOT_PLATFORM_ROOT}/src/drivers/display/ili9342_lcd.c")
endif()

if(CONFIG_MYBOT_ENABLE_VIDEO)
    list(APPEND MYBOT_BOARD_SOURCES "${MYBOT_PLATFORM_ROOT}/src/drivers/video/cores3_camera_video.c")
    list(APPEND MYBOT_BOARD_REQUIRED_CONFIGS
        CONFIG_CAMERA_GC0308_DVP_DEFAULT_FMT_YUV422_YUYV_320X240_20FPS)
endif()
