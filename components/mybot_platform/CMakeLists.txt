idf_build_get_property(_mybot_selected_board MYBOT_BOARD)
if(NOT _mybot_selected_board)
    message(FATAL_ERROR "MYBOT_BOARD build property was not exported by the project")
endif()
set(MYBOT_BOARD "${_mybot_selected_board}")
include("${CMAKE_CURRENT_LIST_DIR}/boards/boards.cmake")
mybot_resolve_board("${MYBOT_BOARD}")
include("${MYBOT_BOARD_PROFILE}")

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    foreach(required_config IN LISTS MYBOT_BOARD_REQUIRED_CONFIGS)
        if(NOT ${required_config})
            message(FATAL_ERROR
                "MYBOT_BOARD=${MYBOT_BOARD} requires ${required_config}=y; "
                "use a clean board-specific sdkconfig")
        endif()
    endforeach()
    if(NOT CONFIG_PARTITION_TABLE_CUSTOM_FILENAME STREQUAL MYBOT_BOARD_PARTITION_TABLE)
        message(FATAL_ERROR
            "MYBOT_BOARD=${MYBOT_BOARD} requires partition table ${MYBOT_BOARD_PARTITION_TABLE}, "
            "got ${CONFIG_PARTITION_TABLE_CUSTOM_FILENAME}")
    endif()
endif()

idf_component_register(
    SRCS
        "src/core/platform.c"
        "src/common/https.c"
        "src/common/kv_store.c"
        "src/common/wifi.cpp"
        ${MYBOT_BOARD_SOURCES}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS
        "src/internal"
        "${MYBOT_BOARD_INCLUDE_DIR}"
    REQUIRES
        mybot_sdk
        aosl
        esp-wifi-connect
        esp-tls
        esp_hw_support
        mbedtls
        nvs_flash
        freertos
        ${MYBOT_BOARD_REQUIRES}
    REQUIRED_IDF_TARGETS esp32s3
)

target_compile_features(${COMPONENT_LIB} PRIVATE c_std_99 cxx_std_17)
