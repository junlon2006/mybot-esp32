# SPDX-License-Identifier: Apache-2.0
# Local GC0308-only production build of esp_cam_sensor 2.3.0.
set(sensor_sources "")
if(CONFIG_MYBOT_ENABLE_VIDEO)
    set(sensor_sources "src/esp_cam_sensor.c" "src/esp_cam_sensor_xclk.c" "src/esp_cam_motor.c"
         "src/esp_cam_sensor_detect.c" "src/driver_dvp/esp_cam_ctlr_dvp_cam.c"
         "sensors/gc0308/gc0308.c")
endif()
idf_component_register(
    SRCS ${sensor_sources}
    INCLUDE_DIRS "include" "sensors/gc0308/include"
    PRIV_INCLUDE_DIRS "sensors/gc0308/private_include"
    REQUIRES esp_sccb_intf esp_driver_cam esp_driver_gpio esp_driver_ledc esp_driver_spi esp_driver_parlio
    PRIV_REQUIRES esp_timer esp_psram esp_mm
    REQUIRED_IDF_TARGETS esp32s3
)
if(CONFIG_MYBOT_ENABLE_VIDEO)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        ESP_CAM_SENSOR_VER_MAJOR=2 ESP_CAM_SENSOR_VER_MINOR=3 ESP_CAM_SENSOR_VER_PATCH=0)
endif()
