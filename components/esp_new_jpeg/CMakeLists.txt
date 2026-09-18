# SPDX-License-Identifier: Apache-2.0
# Local ESP32-S3-only build of esp_new_jpeg 0.6.1.
idf_component_register(INCLUDE_DIRS "include" REQUIRED_IDF_TARGETS esp32s3)
if(CONFIG_MYBOT_ENABLE_VIDEO)
    add_prebuilt_library(esp_new_jpeg_prebuilt "lib/esp32s3/libesp_new_jpeg.a")
    target_link_libraries(${COMPONENT_LIB} INTERFACE esp_new_jpeg_prebuilt)
endif()
