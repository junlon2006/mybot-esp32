/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_RESOURCE_MONITOR_H_
#define MYBOT_RESOURCE_MONITOR_H_

/* Start once from app_main; the diagnostic task lives until reboot. */
int mybot_resource_monitor_start(void);

#endif
