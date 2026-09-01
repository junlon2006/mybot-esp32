/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#ifndef MYBOT_SENSECAP_HARDWARE_H_
#define MYBOT_SENSECAP_HARDWARE_H_

#include "driver/i2c_master.h"
#include "esp_err.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes process-lifetime power, I2C-expander and backlight resources. */
int mybot_sensecap_hardware_init(void);
i2c_master_bus_handle_t mybot_sensecap_i2c_bus_handle(void);

int mybot_sensecap_set_codec_power(bool enabled);
int mybot_sensecap_set_lcd_power(bool enabled);
int mybot_sensecap_set_display_backlight(unsigned int percent);

esp_err_t mybot_sensecap_read_knob_button(bool *released);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_SENSECAP_HARDWARE_H_ */
