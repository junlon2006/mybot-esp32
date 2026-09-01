/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#ifndef MYBOT_CORES3_HARDWARE_H_
#define MYBOT_CORES3_HARDWARE_H_

#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes process-lifetime CoreS3 power, I2C and IO-expander resources. */
int mybot_cores3_hardware_init(void);
i2c_master_bus_handle_t mybot_cores3_i2c_bus_handle(void);
int mybot_cores3_reset_audio_codec(void);
int mybot_cores3_reset_display(void);
int mybot_cores3_set_display_backlight(unsigned int percent);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_CORES3_HARDWARE_H_ */
