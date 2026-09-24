/* SPDX-License-Identifier: MIT */
#ifndef MYBOT_ES8311_BOARD_H_
#define MYBOT_ES8311_BOARD_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void *mybot_es8311_board_i2c_bus_handle(void);
int mybot_es8311_board_set_speaker_power(bool enabled);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_ES8311_BOARD_H_ */
