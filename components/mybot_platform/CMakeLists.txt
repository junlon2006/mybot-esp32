idf_component_register(
    SRCS
        "src/esp32s3_audio.c"
        "src/esp32s3_buttons.c"
        "src/esp32s3_https.c"
        "src/esp32s3_kv_store.c"
        "src/esp32s3_lcd.c"
        "src/esp32s3_platform.c"
        "src/esp32s3_wifi.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        mybot_sdk
        esp-wifi-connect
        button
        esp-tls
        mbedtls
        nvs_flash
        esp_driver_i2s
        esp_driver_gpio
        esp_driver_spi
        esp_lcd
        freertos
    REQUIRED_IDF_TARGETS esp32s3
)

target_compile_features(${COMPONENT_LIB} PRIVATE c_std_99 cxx_std_17)
