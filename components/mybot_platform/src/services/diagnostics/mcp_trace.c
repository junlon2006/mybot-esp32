/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_platform/mcp_trace.h"

#include <api/aosl_log.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TAG "mybot_mcp"
#define RTM_TRACE_CAPACITY 2048

static bool s_started;
static int s_regular_level;

/* Capture existing SDK diagnostics without importing its parser or private interfaces. */
static void trace_sdk_log(int level, const char *format, va_list args) {
    if (!format) {
        return;
    }
    if (!strstr(format, "[RTM]")) {
        if (level <= s_regular_level) {
            va_list copy;
            va_copy(copy, args);
            vprintf(format, copy);
            va_end(copy);
        }
        return;
    }

    /* Each caller owns its buffer; SDK worker stacks stay small and calls may overlap. */
    char *line = heap_caps_malloc(RTM_TRACE_CAPACITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!line) {
        ESP_LOGW(TAG, "event=rtm_trace result=error reason=psram bytes=%d", RTM_TRACE_CAPACITY);
        return;
    }
    va_list copy;
    va_copy(copy, args);
    int length = vsnprintf(line, RTM_TRACE_CAPACITY, format, copy);
    va_end(copy);
    if (length < 0) {
        heap_caps_free(line);
        ESP_LOGW(TAG, "event=rtm_trace result=error reason=format");
        return;
    }
    size_t used = strlen(line);
    while (used && (line[used - 1] == '\n' || line[used - 1] == '\r')) {
        line[--used] = '\0';
    }
    const char *message = strstr(line, "[RTM]");
    if (!message) {
        message = line;
    }
    int truncated = (size_t)length >= RTM_TRACE_CAPACITY;
    if (level <= AOSL_LOG_ERROR) {
        ESP_LOGE(TAG, "event=rtm_trace sdk_level=%d truncated=%d %s", level, truncated, message);
    } else if (level == AOSL_LOG_WARNING) {
        ESP_LOGW(TAG, "event=rtm_trace sdk_level=%d truncated=%d %s", level, truncated, message);
    } else {
        ESP_LOGI(TAG, "event=rtm_trace sdk_level=%d truncated=%d %s", level, truncated, message);
    }
    heap_caps_free(line);
}

void mybot_mcp_trace_start(void) {
    if (s_started) {
        return;
    }
    s_regular_level = aosl_get_log_level();
    if (s_regular_level < AOSL_LOG_NOTICE) {
        s_regular_level = AOSL_LOG_NOTICE;
    }
    s_started = true;
    aosl_set_vlog_func(trace_sdk_log);
    aosl_set_log_level(AOSL_LOG_DEBUG);
    ESP_LOGI(TAG, "event=mcp_trace action=enable rtm=info regular_sdk_level=%d trace_capacity=%d",
             s_regular_level, RTM_TRACE_CAPACITY);
}
