idf_component_register(
    INCLUDE_DIRS
        "include"
        "include/decoder"
        "include/decoder/impl"
    REQUIRED_IDF_TARGETS esp32s3
)

add_prebuilt_library(
    esp_audio_codec_prebuilt
    "${CMAKE_CURRENT_LIST_DIR}/lib/esp32s3/libesp_audio_codec.a"
)
target_link_libraries(${COMPONENT_LIB} INTERFACE esp_audio_codec_prebuilt)
