/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_WIFI_CONTROL_H_
#define MYBOT_WIFI_CONTROL_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*mybot_wifi_provisioning_handler_t)(void);

int mybot_wifi_ensure_network(const char *device_id,
                              mybot_wifi_provisioning_handler_t on_provisioning);
int mybot_wifi_run_provisioning(mybot_wifi_provisioning_handler_t on_provisioning);
void mybot_wifi_shutdown_network(void);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_WIFI_CONTROL_H_ */
