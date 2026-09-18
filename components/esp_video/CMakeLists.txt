# SPDX-License-Identifier: Apache-2.0
# Local ESP32-S3 DVP-only build of esp_video 2.3.0.
set(video_sources "")
if(CONFIG_MYBOT_ENABLE_VIDEO)
    set(video_sources "src/esp_video_buffer.c" "src/esp_video_init.c" "src/esp_video_ioctl.c"
         "src/esp_video_mman.c" "src/esp_video_vfs.c" "src/esp_video.c"
         "src/esp_video_cam.c" "src/device/esp_video_device_common.c"
         "src/device/esp_video_dvp_device.c")
endif()
idf_component_register(
    SRCS ${video_sources}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_include"
    REQUIRES esp_driver_cam esp_cam_sensor lwip
    PRIV_REQUIRES vfs
    REQUIRED_IDF_TARGETS esp32s3
)
if(CONFIG_MYBOT_ENABLE_VIDEO)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        ESP_VIDEO_VER_MAJOR=2 ESP_VIDEO_VER_MINOR=3 ESP_VIDEO_VER_PATCH=0)
endif()
