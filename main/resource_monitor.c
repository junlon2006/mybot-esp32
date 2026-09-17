/* SPDX-License-Identifier: Apache-2.0 */
#include "resource_monitor.h"
#include "mybot_debug_stats.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <inttypes.h>
#include <stdint.h>

#define TAG "mybot_resources"

static TaskHandle_t s_monitor_task;

static void log_heap(const char *region, uint32_t capabilities) {
    multi_heap_info_t info;
    heap_caps_get_info(&info, capabilities);
    ESP_LOGI(TAG,
             "event=memory region=%s unit=bytes used=%zu free=%zu min_free=%zu largest_free=%zu",
             region, info.total_allocated_bytes, info.total_free_bytes, info.minimum_free_bytes,
             info.largest_free_block);
}

static void monitor_task(void *argument) {
    (void)argument;
    configRUN_TIME_COUNTER_TYPE previous_idle[configNUM_CORES];
    configRUN_TIME_COUNTER_TYPE previous_clock = portGET_RUN_TIME_COUNTER_VALUE();
    int64_t previous_us = esp_timer_get_time();
    for (int core = 0; core < configNUM_CORES; ++core) {
        previous_idle[core] = ulTaskGetIdleRunTimeCounterForCore(core);
    }

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR_INTERVAL_MS));
        const configRUN_TIME_COUNTER_TYPE clock = portGET_RUN_TIME_COUNTER_VALUE();
        const int64_t now_us = esp_timer_get_time();
        const uint64_t elapsed = clock - previous_clock;
        configRUN_TIME_COUNTER_TYPE idle_snapshot[configNUM_CORES];
        for (int core = 0; core < configNUM_CORES; ++core) {
            idle_snapshot[core] = ulTaskGetIdleRunTimeCounterForCore(core);
        }
        for (int core = 0; core < configNUM_CORES; ++core) {
            const configRUN_TIME_COUNTER_TYPE idle = idle_snapshot[core];
            const configRUN_TIME_COUNTER_TYPE idle_delta = idle - previous_idle[core];
            /* The kernel omits accounting across runtime-clock rollover. Rebaseline that window. */
            if (clock >= previous_clock && elapsed > 0) {
                const uint64_t idle_ticks = idle_delta < elapsed ? idle_delta : elapsed;
                const unsigned busy_tenths = (unsigned)((elapsed - idle_ticks) * 1000 / elapsed);
                ESP_LOGI(TAG, "event=cpu core=%d busy_pct=%u.%u window_ms=%" PRId64, core,
                         busy_tenths / 10, busy_tenths % 10, (now_us - previous_us) / 1000);
            } else {
                ESP_LOGI(TAG, "event=cpu core=%d result=skipped reason=runtime_clock_wrap", core);
            }
            previous_idle[core] = idle;
        }
        previous_clock = clock;
        previous_us = now_us;
        log_heap("internal", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        log_heap("psram", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        mybot_debug_stats_report();
    }
}

int mybot_resource_monitor_start(void) {
    if (s_monitor_task) {
        return 0;
    }
    if (xTaskCreate(monitor_task, "resource_monitor", 3072, NULL, tskIDLE_PRIORITY + 1,
                    &s_monitor_task) != pdPASS) {
        ESP_LOGE(TAG, "event=resource_monitor action=start result=error reason=no_memory");
        return -1;
    }
    ESP_LOGI(TAG, "event=resource_monitor action=start interval_ms=%d cpu=idle_runtime_delta",
             CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR_INTERVAL_MS);
    return 0;
}
