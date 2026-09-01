# SPDX-License-Identifier: Apache-2.0
# Modified for local vendoring: replace Component Manager metadata with explicit local dependencies.

idf_component_register(
    SRCS "esp_lcd_touch_cst816s.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd_touch esp_lcd
    PRIV_REQUIRES esp_driver_gpio
)
