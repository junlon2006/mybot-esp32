/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_platform/audio_playback_test.h"
#include "announcement/ogg_opus_decoder.h"
#include "audio/pcm_playback_buffer.h"

#include <mybot/platform/mybot_audio.h>
#include <api/aosl.h>
#include <api/aosl_atomic.h>
#include <api/aosl_mpq.h>
#include <api/aosl_mpq_timer.h>
#include <api/aosl_time.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define TAG "mybot_audio_test"
#define TEST_RATE 16000
#define TEST_FRAMES 960
#define TEST_PERIOD_MS 60
#define TEST_TOTAL_FRAMES (TEST_RATE * 60)
#define TEST_TIMEOUT_US (75LL * 1000000)
#define TEST_CLIP_MAX_FRAMES (TEST_RATE * 30)
#define TEST_ASSET_GAP_FRAMES (TEST_RATE / 4)
#define TEST_FADE_FRAMES (TEST_RATE / 200)
#define TEST_WORKER_STACK 8192

const mybot_audio_capture_ops_t *mybot_cores3_audio_capture_ops(void);
const mybot_audio_playback_ops_t *mybot_cores3_audio_playback_ops(void);
const mybot_audio_volume_ops_t *mybot_cores3_audio_volume_ops(void);

#define DECLARE_ASSET(name)                                                                        \
    extern const uint8_t asset_##name##_start[] asm("_binary_" #name "_ogg_start");                \
    extern const uint8_t asset_##name##_end[] asm("_binary_" #name "_ogg_end")
DECLARE_ASSET(prompt);
DECLARE_ASSET(0);
DECLARE_ASSET(1);
DECLARE_ASSET(2);
DECLARE_ASSET(3);
DECLARE_ASSET(4);
DECLARE_ASSET(5);
DECLARE_ASSET(6);
DECLARE_ASSET(7);
DECLARE_ASSET(8);
DECLARE_ASSET(9);

#define ASSET(name)                                                                                \
    { asset_##name##_start, asset_##name##_end }
static const struct {
    const uint8_t *begin;
    const uint8_t *end;
} s_assets[] = {ASSET(prompt), ASSET(0), ASSET(1), ASSET(2), ASSET(3), ASSET(4),
                ASSET(5),      ASSET(6), ASSET(7), ASSET(8), ASSET(9)};

typedef struct {
    uint32_t write_calls;
    uint32_t written_frames;
    uint32_t short_writes;
    uint32_t zero_writes;
    uint32_t errors;
    uint32_t capture_frames;
    uint32_t capture_errors;
    int64_t write_max_us;
    int64_t gap_max_us;
    int64_t late_max_us;
} test_stats_t;

typedef struct {
    const char *name;
    bool timed;
    bool capture;
} test_phase_t;

typedef struct {
    const mybot_audio_playback_ops_t *playback;
    const mybot_audio_capture_ops_t *capture;
    void *playback_ctx;
    void *capture_ctx;
    bool playback_started;
    bool capture_started;
    aosl_mpq_t playback_q;
    aosl_mpq_t capture_q;
    aosl_timer_t playback_timer;
    aosl_timer_t capture_timer;
    aosl_atomic_t stop;
    SemaphoreHandle_t done;
    StaticSemaphore_t done_storage;
    test_phase_t phase;
    const mybot_ogg_pcm_t *clip;
    int clip_offset;
    int pending_offset;
    int pending_frames;
    uint32_t sent_frames;
    int result;
    int16_t playback_frame[TEST_FRAMES];
    int16_t capture_frame[TEST_FRAMES];
    int64_t previous_write_us;
    int64_t next_deadline_us;
    test_stats_t stats;
} test_context_t;

/* Workers own their PCM/cursors; the owner reads only locked statistics until both queues exit. */
static portMUX_TYPE s_stats_lock = portMUX_INITIALIZER_UNLOCKED;
static test_context_t s_test;
static mybot_ogg_pcm_t s_clip;

static int build_clip(mybot_ogg_pcm_t *clip) {
    mybot_ogg_pcm_t decoded[sizeof(s_assets) / sizeof(s_assets[0])] = {0};
    int total = 0;
    int result = -1;
    for (size_t i = 0; i < sizeof(s_assets) / sizeof(s_assets[0]); ++i) {
        if (mybot_ogg_opus_decode(s_assets[i].begin, (size_t)(s_assets[i].end - s_assets[i].begin),
                                  &decoded[i]) < 0 ||
            decoded[i].frames > TEST_CLIP_MAX_FRAMES - total - TEST_ASSET_GAP_FRAMES) {
            goto done;
        }
        total += decoded[i].frames + TEST_ASSET_GAP_FRAMES;
    }
    clip->pcm =
        heap_caps_calloc((size_t)total, sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!clip->pcm) {
        goto done;
    }
    clip->frames = total;
    int offset = 0;
    for (size_t i = 0; i < sizeof(s_assets) / sizeof(s_assets[0]); ++i) {
        const int frames = decoded[i].frames;
        memcpy(clip->pcm + offset, decoded[i].pcm, (size_t)frames * sizeof(int16_t));
        const int fade = frames / 2 < TEST_FADE_FRAMES ? frames / 2 : TEST_FADE_FRAMES;
        for (int n = 0; n < fade; ++n) {
            clip->pcm[offset + n] = (int16_t)((int32_t)clip->pcm[offset + n] * n / fade);
            clip->pcm[offset + frames - 1 - n] =
                (int16_t)((int32_t)clip->pcm[offset + frames - 1 - n] * n / fade);
        }
        offset += frames + TEST_ASSET_GAP_FRAMES;
    }
    result = 0;
done:
    for (size_t i = 0; i < sizeof(s_assets) / sizeof(s_assets[0]); ++i) {
        mybot_ogg_pcm_free(&decoded[i]);
    }
    return result;
}

static void finish_playback(test_context_t *test, int result) {
    test->result = result;
    aosl_atomic_set(&test->stop, true);
    xSemaphoreGive(test->done);
}

static void write_block(test_context_t *test) {
    if (aosl_atomic_read(&test->stop)) {
        return;
    }
    if (!test->pending_frames) {
        for (int n = 0; n < TEST_FRAMES; ++n) {
            test->playback_frame[n] = test->clip->pcm[test->clip_offset++];
            if (test->clip_offset == test->clip->frames) {
                test->clip_offset = 0;
            }
        }
        test->pending_offset = 0;
        test->pending_frames = TEST_FRAMES;
    }

    const int64_t begin = esp_timer_get_time();
    int written = test->playback->write(
        test->playback_ctx, test->playback_frame + test->pending_offset, test->pending_frames);
    const int64_t duration = esp_timer_get_time() - begin;
    const bool error = written < 0 || written > test->pending_frames;
    portENTER_CRITICAL(&s_stats_lock);
    test->stats.write_calls++;
    test->stats.errors += error;
    test->stats.zero_writes += written == 0;
    test->stats.short_writes += !error && written < test->pending_frames;
    if (!error) {
        test->stats.written_frames += written;
    }
    if (duration > test->stats.write_max_us) {
        test->stats.write_max_us = duration;
    }
    if (test->previous_write_us && begin - test->previous_write_us > test->stats.gap_max_us) {
        test->stats.gap_max_us = begin - test->previous_write_us;
    }
    if (test->phase.timed && begin - test->next_deadline_us > test->stats.late_max_us) {
        test->stats.late_max_us = begin - test->next_deadline_us;
    }
    portEXIT_CRITICAL(&s_stats_lock);
    test->previous_write_us = begin;
    test->next_deadline_us += TEST_PERIOD_MS * 1000;
    if (error) {
        finish_playback(test, -1);
        return;
    }
    test->pending_offset += written;
    test->pending_frames -= written;
    test->sent_frames += written;
    if (test->sent_frames == TEST_TOTAL_FRAMES) {
        finish_playback(test, 0);
    } else if (!written) {
        /* An abnormal zero-progress driver must not busy-spin in continuous mode. */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void playback_timer_cb(aosl_timer_t id, const aosl_ts_t *now, uintptr_t argc,
                              uintptr_t argv[]) {
    (void)id;
    (void)now;
    (void)argc;
    write_block((test_context_t *)argv[0]);
}

static void continuous_cb(const aosl_ts_t *ts, aosl_refobj_t ref, uintptr_t argc,
                          uintptr_t argv[]) {
    (void)ts;
    (void)ref;
    (void)argc;
    test_context_t *test = (test_context_t *)argv[0];
    while (!aosl_atomic_read(&test->stop)) {
        write_block(test);
    }
}

static int playback_worker_init(void *arg) {
    test_context_t *test = arg;
    if (!test->phase.timed) {
        return aosl_mpq_queue(aosl_mpq_this(), AOSL_MPQ_INVALID, AOSL_REF_INVALID,
                              "audio_test_stream", continuous_cb, 1, (uintptr_t)test);
    }
    test->next_deadline_us = esp_timer_get_time() + TEST_PERIOD_MS * 1000;
    test->playback_timer =
        aosl_mpq_set_timer(TEST_PERIOD_MS, playback_timer_cb, NULL, 1, (uintptr_t)test);
    return aosl_mpq_timer_invalid(test->playback_timer) ? -1 : 0;
}

static void playback_worker_fini(void *arg) {
    test_context_t *test = arg;
    if (!aosl_mpq_timer_invalid(test->playback_timer)) {
        aosl_mpq_kill_timer(test->playback_timer);
    }
}

static void capture_timer_cb(aosl_timer_t id, const aosl_ts_t *now, uintptr_t argc,
                             uintptr_t argv[]) {
    (void)id;
    (void)now;
    (void)argc;
    test_context_t *test = (test_context_t *)argv[0];
    if (aosl_atomic_read(&test->stop)) {
        return;
    }
    int frames = test->capture->read(test->capture_ctx, test->capture_frame, TEST_FRAMES);
    portENTER_CRITICAL(&s_stats_lock);
    if (frames >= 0 && frames <= TEST_FRAMES) {
        test->stats.capture_frames += frames;
    } else {
        test->stats.capture_errors++;
    }
    portEXIT_CRITICAL(&s_stats_lock);
}

static int capture_worker_init(void *arg) {
    test_context_t *test = arg;
    test->capture_timer =
        aosl_mpq_set_timer(TEST_PERIOD_MS, capture_timer_cb, NULL, 1, (uintptr_t)test);
    return aosl_mpq_timer_invalid(test->capture_timer) ? -1 : 0;
}

static void capture_worker_fini(void *arg) {
    test_context_t *test = arg;
    if (!aosl_mpq_timer_invalid(test->capture_timer)) {
        aosl_mpq_kill_timer(test->capture_timer);
    }
}

static test_stats_t report_progress(test_context_t *test) {
    portENTER_CRITICAL(&s_stats_lock);
    const test_stats_t stats = test->stats;
    portEXIT_CRITICAL(&s_stats_lock);
    ESP_LOGI(TAG,
             "event=progress phase=%s written_frames=%" PRIu32 " write_calls=%" PRIu32
             " short_writes=%" PRIu32 " zero_writes=%" PRIu32 " errors=%" PRIu32
             " write_max_us=%" PRId64 " gap_max_us=%" PRId64 " late_max_us=%" PRId64
             " capture_frames=%" PRIu32 " capture_errors=%" PRIu32,
             test->phase.name, stats.written_frames, stats.write_calls, stats.short_writes,
             stats.zero_writes, stats.errors, stats.write_max_us, stats.gap_max_us,
             stats.late_max_us, stats.capture_frames, stats.capture_errors);
    return stats;
}

static int stop_phase(test_context_t *test) {
    aosl_atomic_set(&test->stop, true);
    /* I/O waits are bounded at 50 ms in the CoreS3 adapter. Join workers before freeing contexts.
     */
    if (!aosl_mpq_invalid(test->playback_q)) {
        if (aosl_mpq_destroy_wait(test->playback_q) < 0) {
            return -1;
        }
        test->playback_q = AOSL_MPQ_INVALID;
    }
    if (!aosl_mpq_invalid(test->capture_q)) {
        if (aosl_mpq_destroy_wait(test->capture_q) < 0) {
            return -1;
        }
        test->capture_q = AOSL_MPQ_INVALID;
    }
    /* A successful write now admits PCM to the bounded software FIFO. Wait for its tail too. */
    if (test->playback_started && test->result == 0 &&
        mybot_audio_playback_drain(test->playback_ctx, 1000) < 0) {
        test->result = -1;
        ESP_LOGE(TAG, "event=phase phase=%s reason=drain_failed", test->phase.name);
    }
    if (test->playback_started && test->playback->stop(test->playback_ctx) < 0) {
        return -1;
    }
    if (test->capture_started && test->capture->stop(test->capture_ctx) < 0) {
        return -1;
    }
    if (test->playback_ctx) {
        test->playback->destroy(test->playback_ctx);
    }
    if (test->capture_ctx) {
        test->capture->destroy(test->capture_ctx);
    }
    return 0;
}

static int run_phase(test_phase_t phase) {
    test_context_t *test = &s_test;
    *test = (test_context_t){
        .phase = phase,
        .clip = &s_clip,
        .playback = mybot_cores3_audio_playback_ops(),
        .capture = mybot_cores3_audio_capture_ops(),
        .playback_q = AOSL_MPQ_INVALID,
        .capture_q = AOSL_MPQ_INVALID,
        .playback_timer = AOSL_MPQ_TIMER_INVALID,
        .capture_timer = AOSL_MPQ_TIMER_INVALID,
        .result = -1,
    };
    test->done = xSemaphoreCreateBinaryStatic(&test->done_storage);
    ESP_LOGI(TAG, "event=phase phase=%s action=begin audio_seconds=60 capture=%d block_frames=%d",
             phase.name, phase.capture, TEST_FRAMES);
    int result = -1;
    const int64_t started = esp_timer_get_time();
    if (phase.capture && test->capture->init(&test->capture_ctx, TEST_RATE, 1, 16) < 0) {
        goto cleanup;
    }
    if (test->playback->init(&test->playback_ctx, TEST_RATE, 1, 16) < 0) {
        goto cleanup;
    }
    if (phase.capture) {
        if (test->capture->start(test->capture_ctx) < 0) {
            goto cleanup;
        }
        test->capture_started = true;
    }
    if (test->playback->start(test->playback_ctx) < 0) {
        goto cleanup;
    }
    test->playback_started = true;
    if (phase.capture) {
        test->capture_q =
            aosl_mpq_create_flags(AOSL_MPQ_FLAG_SIGP_EVENT, AOSL_THRD_PRI_NORMAL, TEST_WORKER_STACK,
                                  8, "test_cap", capture_worker_init, capture_worker_fini, test);
        if (aosl_mpq_invalid(test->capture_q)) {
            goto cleanup;
        }
    }
    /* Match the real idle-before-speech case. Both A and B start with TX running, no preload. */
    vTaskDelay(pdMS_TO_TICKS(500));
    test->playback_q =
        aosl_mpq_create_flags(AOSL_MPQ_FLAG_SIGP_EVENT, AOSL_THRD_PRI_NORMAL, TEST_WORKER_STACK, 8,
                              "test_pb", playback_worker_init, playback_worker_fini, test);
    if (aosl_mpq_invalid(test->playback_q)) {
        goto cleanup;
    }
    for (;;) {
        int64_t remaining = TEST_TIMEOUT_US - (esp_timer_get_time() - started);
        if (remaining <= 0) {
            ESP_LOGE(TAG, "event=phase phase=%s reason=deadline", phase.name);
            break;
        }
        uint32_t wait_ms = (uint32_t)((remaining + 999) / 1000);
        if (wait_ms > 5000) {
            wait_ms = 5000;
        }
        bool done = xSemaphoreTake(test->done, pdMS_TO_TICKS(wait_ms)) == pdTRUE;
        if (done) {
            result = test->result;
            break;
        }
        test_stats_t stats = report_progress(test);
        if (stats.capture_errors) {
            ESP_LOGE(TAG, "event=phase phase=%s reason=capture_error", phase.name);
            break;
        }
    }
cleanup:
    if (stop_phase(test) < 0) {
        ESP_LOGE(TAG, "event=phase phase=%s action=failed reason=cleanup resources=retained",
                 phase.name);
        return -2;
    }
    /* A capture read may have been in flight when playback signalled completion. */
    test_stats_t stats = report_progress(test);
    if (test->result < 0) {
        result = -1;
    }
    if (phase.capture && (stats.capture_errors || !stats.capture_frames)) {
        ESP_LOGE(TAG, "event=phase phase=%s reason=invalid_capture", phase.name);
        result = -1;
    }
    ESP_LOGI(TAG, "event=phase phase=%s action=%s elapsed_ms=%" PRId64, phase.name,
             result == 0 ? "complete" : "failed", (esp_timer_get_time() - started) / 1000);
    return result;
}

int mybot_audio_playback_test_run(void) {
    static const test_phase_t phases[] = {
        {"A_timer_capture", true, true},
        {"B_stream_capture", false, true},
        {"C_stream_only", false, false},
    };
    const mybot_audio_volume_ops_t *volume = mybot_cores3_audio_volume_ops();
    void *volume_ctx = NULL;
    int level = 0;
    int result = -1;
    ESP_LOGI(TAG, "event=test action=begin language=%s rate=16000 sdk=off wifi=off",
             CONFIG_MYBOT_LANGUAGE_TAG);
    /* Normal startup acquires this runtime in mybot_start(), which this test bypasses. */
    aosl_ctor();
    if (volume->init(&volume_ctx) < 0) {
        ESP_LOGE(TAG, "event=test action=failed reason=volume_init");
        aosl_dtor();
        return -1;
    }
    if (volume->get_volume(volume_ctx, &level) < 0 || build_clip(&s_clip) < 0) {
        ESP_LOGE(TAG, "event=test action=failed reason=prepare_pcm_or_volume");
        goto done;
    }
    ESP_LOGI(
        TAG,
        "event=test action=ready volume=%d clip_frames=%d repeats=loop dma_underruns=unmeasured",
        level, s_clip.frames);
    for (size_t i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        result = run_phase(phases[i]);
        if (result == -2) {
            /* Static test/clip storage stays alive if worker teardown cannot be proven. */
            ESP_LOGE(TAG, "event=test action=failed reason=cleanup reboot_required=1");
            return -1;
        }
        if (result < 0) {
            break;
        }
    }
done:
    mybot_ogg_pcm_free(&s_clip);
    volume->destroy(volume_ctx);
    aosl_dtor();
    ESP_LOGI(TAG, "event=test action=%s normal_startup=disabled",
             result == 0 ? "complete" : "failed");
    return result;
}
