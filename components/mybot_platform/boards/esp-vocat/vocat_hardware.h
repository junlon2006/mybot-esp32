/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#ifndef MYBOT_VOCAT_HARDWARE_H_
#define MYBOT_VOCAT_HARDWARE_H_

#include "driver/gpio.h"
#include "driver/i2c_master.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int mybot_vocat_hardware_init(void);
int mybot_vocat_hardware_deinit(void);
i2c_master_bus_handle_t mybot_vocat_i2c_bus_handle(void);
gpio_num_t mybot_vocat_audio_din_gpio(void);
gpio_num_t mybot_vocat_audio_pa_gpio(void);
gpio_num_t mybot_vocat_lcd_reset_gpio(void);
bool mybot_vocat_lcd_reset_active_high(void);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_VOCAT_HARDWARE_H_ */
