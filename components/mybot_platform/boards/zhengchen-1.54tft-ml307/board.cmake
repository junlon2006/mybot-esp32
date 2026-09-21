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
)
set(MYBOT_BOARD_PARTITION_TABLE "partitions/v2/16m.csv")

get_filename_component(MYBOT_PLATFORM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${MYBOT_PLATFORM_ROOT}/src/drivers/display/display.cmake")
set(MYBOT_BOARD_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../zhengchen-1.54tft-common/board.c"
    "${MYBOT_PLATFORM_ROOT}/src/drivers/audio/raw_i2s_audio.c"
    "${MYBOT_PLATFORM_ROOT}/src/drivers/input/gpio_buttons.c"
)
mybot_display_add_lvgl_sources(MYBOT_BOARD_SOURCES "st7789_lvgl_adapter.cc")
set(MYBOT_BOARD_REQUIRES
    button
    esp_driver_gpio
    esp_driver_i2s
    esp_driver_spi
    esp_lcd
    esp_timer
)
list(APPEND MYBOT_BOARD_REQUIRES esp_lvgl_port)
