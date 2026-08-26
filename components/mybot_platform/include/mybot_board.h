/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_BOARD_H_
#define MYBOT_BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *id;
    const char *hw_model;
    int (*prepare)(void);
    int (*register_platform)(void);
} mybot_board_t;

const mybot_board_t *mybot_board_get(void);
int mybot_board_register(void);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_BOARD_H_ */
