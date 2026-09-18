# SPDX-License-Identifier: Apache-2.0
# Local I2C-only build of esp_sccb_intf 0.0.8.
set(sccb_sources "")
if(CONFIG_MYBOT_ENABLE_VIDEO)
    set(sccb_sources "src/sccb.c" "sccb_i2c/src/sccb_i2c.c")
endif()
idf_component_register(
    SRCS ${sccb_sources}
    INCLUDE_DIRS "include" "interface" "sccb_i2c/include"
    REQUIRES esp_driver_i2c
)
