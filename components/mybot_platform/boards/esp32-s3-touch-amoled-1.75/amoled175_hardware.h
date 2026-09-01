/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#ifndef MYBOT_AMOLED175_HARDWARE_H_
#define MYBOT_AMOLED175_HARDWARE_H_

#include "driver/i2c_master.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int mybot_amoled175_hardware_init(void);
int mybot_amoled175_hardware_deinit(void);
i2c_master_bus_handle_t mybot_amoled175_i2c_bus_handle(void);
int mybot_amoled175_set_speaker_pa(bool enabled);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_AMOLED175_HARDWARE_H_ */
