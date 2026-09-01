idf_component_register(
    SRCS "esp_lcd_spd2010.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd
    PRIV_REQUIRES esp_driver_gpio
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    ESP_LCD_SPD2010_VER_MAJOR=2
    ESP_LCD_SPD2010_VER_MINOR=0
    ESP_LCD_SPD2010_VER_PATCH=0
)
