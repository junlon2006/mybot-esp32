/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd. */
/* Copyright (c) 2025 Project Contributors */
#ifndef MYBOT_XVF3800_HARDWARE_H_
#define MYBOT_XVF3800_HARDWARE_H_

#include "esp_err.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes process-lifetime XVF3800 and AIC3104 control resources. */
int mybot_xvf3800_hardware_init(void);

/* Returns ESP_ERR_NOT_FINISHED while the XVF3800 control servicer is busy. */
esp_err_t mybot_xvf3800_read_button(bool *released);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_XVF3800_HARDWARE_H_ */
