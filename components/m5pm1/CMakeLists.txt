# SPDX-License-Identifier: MIT
# Copyright (c) 2025 M5Stack Technology CO LTD

idf_component_register(
    SRCS "src/M5PM1.cpp"
    INCLUDE_DIRS "src"
    REQUIRES
        esp_driver_i2c
        esp_rom
        freertos
        log
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
