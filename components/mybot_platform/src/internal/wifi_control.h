/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_WIFI_CONTROL_H_
#define MYBOT_WIFI_CONTROL_H_

#ifdef __cplusplus
extern "C" {
#endif

int mybot_wifi_ensure_network(const char *device_id);
int mybot_wifi_run_provisioning(void);
void mybot_wifi_shutdown_network(void);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_WIFI_CONTROL_H_ */
