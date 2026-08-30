set(component_srcs
    "esp_codec_dev.c"
    "esp_codec_dev_vol.c"
    "esp_codec_dev_if.c"
    "audio_codec_sw_vol.c"
    "platform/audio_codec_gpio.c"
    "platform/audio_codec_ctrl_i2c.c"
    "platform/audio_codec_ctrl_spi.c"
    "platform/audio_codec_data_i2s.c"
    "platform/esp_codec_dev_os.c"
)

if(CONFIG_CODEC_ES7210_SUPPORT)
    list(APPEND component_srcs "device/es7210/es7210.c")
endif()
if(CONFIG_CODEC_AW88298_SUPPORT)
    list(APPEND component_srcs "device/aw88298/aw88298.c")
endif()

idf_component_register(
    SRCS ${component_srcs}
    INCLUDE_DIRS "include" "interface" "device/include"
    PRIV_INCLUDE_DIRS "device/priv_include"
    PRIV_REQUIRES
        freertos
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_i2s
        esp_driver_spi
)
