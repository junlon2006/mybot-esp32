idf_component_register(
    SRCS "app_main.c"
    REQUIRES
        mybot_sdk
        mybot_platform
        agora_rtc
        aosl
        nvs_flash
        esp_psram
        esp_app_format
        esp_netif
        esp_event
)
