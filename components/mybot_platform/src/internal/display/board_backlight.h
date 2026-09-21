/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_BOARD_BACKLIGHT_H_
#define MYBOT_BOARD_BACKLIGHT_H_

#ifdef __cplusplus
extern "C" {
#endif

/* The board owns GPIO/PWM setup; the renderer must not change its pin routing. */
int mybot_board_set_display_backlight(unsigned int percent);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_BOARD_BACKLIGHT_H_ */
