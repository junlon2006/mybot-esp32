/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#ifndef MYBOT_STICKS3_HARDWARE_H_
#define MYBOT_STICKS3_HARDWARE_H_

#include "driver/i2c_master.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes process-lifetime StickS3 I2C, power-rail and backlight resources. */
int mybot_sticks3_hardware_init(void);
/* Rolls back hardware resources when Board preparation fails. */
int mybot_sticks3_hardware_deinit(void);
i2c_master_bus_handle_t mybot_sticks3_i2c_bus_handle(void);
int mybot_sticks3_set_speaker_power(bool enabled);
int mybot_sticks3_set_display_backlight(unsigned int percent);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_STICKS3_HARDWARE_H_ */
