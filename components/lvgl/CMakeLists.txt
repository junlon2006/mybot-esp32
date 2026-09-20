# SPDX-License-Identifier: Apache-2.0
# Local build selection for the pinned LVGL 9.5.0 production subset.
set(lvgl_sources "")
if(CONFIG_MYBOT_CORES3_LVGL_UI)
    file(GLOB_RECURSE lvgl_sources "src/*.c")
    list(APPEND lvgl_sources "local/lv_mem_psram.c")
endif()

idf_component_register(SRCS ${lvgl_sources}
    INCLUDE_DIRS "." "src"
    PRIV_REQUIRES heap esp_timer log)
if(CONFIG_MYBOT_CORES3_LVGL_UI)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC LV_CONF_INCLUDE_SIMPLE LV_KCONFIG_IGNORE)
endif()
