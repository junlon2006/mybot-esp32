# SPDX-License-Identifier: Apache-2.0
# Common display source composition for board profiles.

set(MYBOT_DISPLAY_ROOT "${MYBOT_PLATFORM_ROOT}/src/drivers/display")

function(mybot_display_add_panel source_var source_name)
    list(APPEND ${source_var} "${MYBOT_DISPLAY_ROOT}/panels/${source_name}")
    set(${source_var} "${${source_var}}" PARENT_SCOPE)
endfunction()

function(mybot_display_add_legacy_renderer source_var source_name)
    list(APPEND ${source_var} "${MYBOT_DISPLAY_ROOT}/renderers/legacy/${source_name}")
    set(${source_var} "${${source_var}}" PARENT_SCOPE)
endfunction()

function(mybot_display_add_lvgl_sources source_var adapter_name)
    list(APPEND ${source_var}
        "${MYBOT_DISPLAY_ROOT}/adapters/lvgl/${adapter_name}"
        "${MYBOT_DISPLAY_ROOT}/renderers/lvgl/lvgl_view.cc"
        "${MYBOT_DISPLAY_ROOT}/renderers/lvgl/lvgl_assets.c"
        "${MYBOT_DISPLAY_ROOT}/renderers/lvgl/lvgl_fonts.c")
    set(${source_var} "${${source_var}}" PARENT_SCOPE)
endfunction()

function(mybot_display_add_lvgl_bridge source_var source_name)
    list(APPEND ${source_var} "${MYBOT_DISPLAY_ROOT}/adapters/lvgl/${source_name}")
    set(${source_var} "${${source_var}}" PARENT_SCOPE)
endfunction()
