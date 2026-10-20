/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MCP_TRACE_MOCK_AOSL_LOG_H_
#define MCP_TRACE_MOCK_AOSL_LOG_H_

#include <stdarg.h>

#define AOSL_LOG_EMERG 0
#define AOSL_LOG_ALERT 1
#define AOSL_LOG_CRIT 2
#define AOSL_LOG_ERROR 3
#define AOSL_LOG_WARNING 4
#define AOSL_LOG_NOTICE 5
#define AOSL_LOG_INFO 6
#define AOSL_LOG_DEBUG 7

typedef void (*aosl_vlog_t)(int level, const char *format, va_list args);

int aosl_get_log_level(void);
void aosl_set_log_level(int level);
void aosl_set_vlog_func(aosl_vlog_t callback);

#endif /* MCP_TRACE_MOCK_AOSL_LOG_H_ */
