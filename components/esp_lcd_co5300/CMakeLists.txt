# SPDX-License-Identifier: Apache-2.0
# Modified for local vendoring: keep the ESP32-S3 SPI/QSPI path and define its version locally.

idf_component_register(
    SRCS "esp_lcd_co5300.c" "esp_lcd_co5300_spi.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES esp_lcd
    PRIV_REQUIRES esp_driver_gpio
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    ESP_LCD_CO5300_VER_MAJOR=2
    ESP_LCD_CO5300_VER_MINOR=1
    ESP_LCD_CO5300_VER_PATCH=0
)
