set(MYBOT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../third_party/mybot")

set(MYBOT_VERSION_MAJOR 1)
set(MYBOT_VERSION_MINOR 0)
set(MYBOT_VERSION_PATCH 0)
set(MYBOT_VERSION_PRERELEASE "")
set(MYBOT_VERSION_STRING "1.0.0")

set(MYBOT_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    file(MAKE_DIRECTORY "${MYBOT_GENERATED_INCLUDE_DIR}/mybot")
    configure_file(
        "${MYBOT_ROOT}/include/mybot/mybot_version.h.in"
        "${MYBOT_GENERATED_INCLUDE_DIR}/mybot/mybot_version.h"
        @ONLY
    )
endif()

set(MYBOT_SOURCES
    "${MYBOT_ROOT}/src/core/mybot_app.c"
    "${MYBOT_ROOT}/src/core/mybot_device_lifecycle.c"
    "${MYBOT_ROOT}/src/core/mybot_presenter.c"
    "${MYBOT_ROOT}/src/core/mybot_state_model.c"
    "${MYBOT_ROOT}/src/core/mybot_version.c"
    "${MYBOT_ROOT}/src/media/mybot_audio.c"
    "${MYBOT_ROOT}/src/media/mybot_media_pipeline.c"
    "${MYBOT_ROOT}/src/media/mybot_wake_words.c"
    "${MYBOT_ROOT}/src/platform/mybot_announce.c"
    "${MYBOT_ROOT}/src/platform/mybot_key.c"
    "${MYBOT_ROOT}/src/platform/mybot_https.c"
    "${MYBOT_ROOT}/src/platform/mybot_kv_store.c"
    "${MYBOT_ROOT}/src/platform/mybot_lcd.c"
    "${MYBOT_ROOT}/src/platform/mybot_platform.c"
    "${MYBOT_ROOT}/src/platform/mybot_wifi.c"
    "${MYBOT_ROOT}/src/rtc/mybot_agora_rtc.c"
    "${MYBOT_ROOT}/src/service/mybot_device_client.c"
    "${MYBOT_ROOT}/src/support/mybot_http_client.c"
    "${MYBOT_ROOT}/src/support/mybot_json.c"
    "${MYBOT_ROOT}/src/support/mybot_ringbuf.c"
)

idf_component_register(
    SRCS ${MYBOT_SOURCES}
    INCLUDE_DIRS
        "${MYBOT_GENERATED_INCLUDE_DIR}"
        "${MYBOT_ROOT}/include"
    PRIV_INCLUDE_DIRS "${MYBOT_ROOT}/src/internal"
    PRIV_REQUIRES aosl agora_rtc
    REQUIRED_IDF_TARGETS esp32s3
)

target_compile_features(${COMPONENT_LIB} PRIVATE c_std_99)
target_compile_options(${COMPONENT_LIB} PRIVATE -fvisibility=hidden)
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE MYBOT_BUILDING_LIBRARY
    PUBLIC
        MYBOT_AUDIO_PTIME_MS=${CONFIG_MYBOT_AUDIO_PTIME_MS}
        MYBOT_CLOUD_AEC=$<BOOL:${CONFIG_MYBOT_CLOUD_AEC}>
        MYBOT_WAKE_WORDS=0
        MYBOT_AI_QOS=$<BOOL:${CONFIG_MYBOT_AI_QOS}>
        MYBOT_FAST_SEND_MULTIPLIER=${CONFIG_MYBOT_FAST_SEND_MULTIPLIER}
        MYBOT_ENABLE_HTTPS=1
        MYBOT_ALLOW_INSECURE_HTTP=0
)
