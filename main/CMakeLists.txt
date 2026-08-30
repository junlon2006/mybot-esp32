if(CONFIG_LWIP_MAX_SOCKETS LESS 24)
    message(FATAL_ERROR
        "mybot requires CONFIG_LWIP_MAX_SOCKETS >= 24 (recommended: 32); "
        "remove the stale sdkconfig and reconfigure")
endif()

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
