/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_debug_stats.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include <inttypes.h>

typedef struct {
    uint32_t calls;
    uint32_t requested;
    uint32_t written;
    uint32_t timeouts;
    uint32_t errors;
    uint32_t short_writes;
    uint32_t zero_writes;
    int64_t write_us;
    int64_t max_write_us;
    int64_t max_write_at_us;
    int64_t max_start_gap_us;
    int64_t max_idle_gap_us;
    int64_t max_gap_at_us;
} playback_stats_t;

typedef struct {
    uint32_t calls;
    uint32_t errors;
    int64_t render_us;
    int64_t flush_us;
    int64_t max_render_us;
    int64_t max_flush_us;
    int64_t max_total_us;
    int64_t max_total_at_us;
} ui_stats_t;

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static playback_stats_t s_playback;
static ui_stats_t s_ui;

void mybot_debug_record_playback(uint32_t requested, uint32_t written, bool timeout, bool error,
                                 int64_t begin, int64_t end, int64_t previous_begin,
                                 int64_t previous_end) {
    const int64_t duration = end - begin;
    portENTER_CRITICAL(&s_lock);
    ++s_playback.calls;
    s_playback.requested += requested;
    s_playback.written += written;
    s_playback.timeouts += timeout;
    s_playback.errors += error;
    s_playback.short_writes += written < requested;
    s_playback.zero_writes += written == 0;
    s_playback.write_us += duration;
    if (duration > s_playback.max_write_us) {
        s_playback.max_write_us = duration;
        s_playback.max_write_at_us = begin;
    }
    if (previous_begin > 0 && begin - previous_begin > s_playback.max_start_gap_us) {
        s_playback.max_start_gap_us = begin - previous_begin;
    }
    if (previous_end > 0 && begin - previous_end > s_playback.max_idle_gap_us) {
        s_playback.max_idle_gap_us = begin - previous_end;
        s_playback.max_gap_at_us = begin;
    }
    portEXIT_CRITICAL(&s_lock);
}

void mybot_debug_record_ui(int64_t begin, int64_t rendered, int64_t end, bool error) {
    portENTER_CRITICAL(&s_lock);
    ++s_ui.calls;
    s_ui.errors += error;
    s_ui.render_us += rendered - begin;
    s_ui.flush_us += end - rendered;
    if (rendered - begin > s_ui.max_render_us) {
        s_ui.max_render_us = rendered - begin;
    }
    if (end - rendered > s_ui.max_flush_us) {
        s_ui.max_flush_us = end - rendered;
    }
    if (end - begin > s_ui.max_total_us) {
        s_ui.max_total_us = end - begin;
        s_ui.max_total_at_us = begin;
    }
    portEXIT_CRITICAL(&s_lock);
}

void mybot_debug_stats_report(void) {
    portENTER_CRITICAL(&s_lock);
    const playback_stats_t playback = s_playback;
    const ui_stats_t ui = s_ui;
    s_playback = (playback_stats_t){0};
    s_ui = (ui_stats_t){0};
    portEXIT_CRITICAL(&s_lock);

    /* Counters cover this report interval; *_at_ms is monotonic time since boot. */
    ESP_LOGI("mybot_stats",
             "event=playback_stats calls=%" PRIu32 " requested_frames=%" PRIu32
             " written_frames=%" PRIu32 " timeouts=%" PRIu32 " errors=%" PRIu32
             " short_writes=%" PRIu32 " zero_writes=%" PRIu32 " write_avg_us=%" PRId64
             " write_max_us=%" PRId64 " write_max_at_ms=%" PRId64,
             playback.calls, playback.requested, playback.written, playback.timeouts,
             playback.errors, playback.short_writes, playback.zero_writes,
             playback.calls ? playback.write_us / playback.calls : 0, playback.max_write_us,
             playback.max_write_at_us / 1000);
    ESP_LOGI("mybot_stats",
             "event=playback_gaps start_gap_max_us=%" PRId64 " idle_gap_max_us=%" PRId64
             " idle_gap_at_ms=%" PRId64 " sdk_queue_underruns=unavailable",
             playback.max_start_gap_us, playback.max_idle_gap_us, playback.max_gap_at_us / 1000);
    ESP_LOGI("mybot_stats",
             "event=ui_stats calls=%" PRIu32 " errors=%" PRIu32 " render_avg_us=%" PRId64
             " render_max_us=%" PRId64 " flush_avg_us=%" PRId64 " flush_max_us=%" PRId64
             " total_max_us=%" PRId64 " total_max_at_ms=%" PRId64,
             ui.calls, ui.errors, ui.calls ? ui.render_us / ui.calls : 0, ui.max_render_us,
             ui.calls ? ui.flush_us / ui.calls : 0, ui.max_flush_us, ui.max_total_us,
             ui.max_total_at_us / 1000);
}
