/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_BOARD_H_
#define MYBOT_BOARD_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *id;
    const char *hw_model;
    int (*prepare)(void);
    int (*register_platform)(void);
    /* Network ownership stays outside mybot so connectivity is a startup prerequisite. */
    int (*ensure_network)(const char *device_id);
    int (*provision_wifi)(void);
    void (*shutdown_network)(void);
} mybot_board_t;

const mybot_board_t *mybot_board_get(void);
int mybot_board_register(void);
void mybot_board_request_wifi_provisioning(void);
bool mybot_board_take_wifi_provisioning_request(void);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_BOARD_H_ */
