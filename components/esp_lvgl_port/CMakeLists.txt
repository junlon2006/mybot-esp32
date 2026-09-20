# SPDX-License-Identifier: Apache-2.0
# Local LVGL9 display/runtime subset of esp_lvgl_port 2.8.0~1.
set(lvgl_port_sources "")
if(CONFIG_MYBOT_CORES3_LVGL_UI)
    list(APPEND lvgl_port_sources "src/lvgl9/esp_lvgl_port.c" "src/lvgl9/esp_lvgl_port_disp.c")
endif()

idf_component_register(SRCS ${lvgl_port_sources}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES lvgl esp_lcd
    PRIV_REQUIRES esp_timer esp_system heap)
