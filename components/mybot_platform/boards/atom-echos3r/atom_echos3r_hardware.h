/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Project Contributors */
#ifndef MYBOT_ATOM_ECHOS3R_HARDWARE_H_
#define MYBOT_ATOM_ECHOS3R_HARDWARE_H_

#include "driver/i2c_master.h"

#include <stdbool.h>

int mybot_atom_echos3r_hardware_init(void);
int mybot_atom_echos3r_hardware_deinit(void);
i2c_master_bus_handle_t mybot_atom_echos3r_i2c_bus_handle(void);
int mybot_atom_echos3r_set_speaker_power(bool enabled);

#endif /* MYBOT_ATOM_ECHOS3R_HARDWARE_H_ */
