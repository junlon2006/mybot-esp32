/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_PLATFORM_DEBUG_STATS_H_
#define MYBOT_PLATFORM_DEBUG_STATS_H_

#include <stdbool.h>
#include <stdint.h>

/* Only linked when CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR is enabled. Times are microseconds. */
void mybot_debug_record_playback(uint32_t requested, uint32_t written, bool timeout, bool error,
                                 int64_t begin, int64_t end, int64_t previous_begin,
                                 int64_t previous_end);
void mybot_debug_record_ui(int64_t begin, int64_t rendered, int64_t end, bool error);
void mybot_debug_stats_report(void);

#endif
