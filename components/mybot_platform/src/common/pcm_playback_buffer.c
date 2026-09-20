/* SPDX-License-Identifier: Apache-2.0 */
#include "pcm_playback_buffer.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#if CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR
#include "mybot_debug_stats.h"
#endif

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define TAG "mybot_playback"
#define PCM_RATE 16000
#define FIFO_FRAMES 3840
#define PREBUFFER_FRAMES 1920
#define WRITE_FRAMES 240
#define PREBUFFER_TIMEOUT_MS 100
#define PRODUCER_TIMEOUT_MS 50
#define I2S_TIMEOUT_MS 50
#define STOP_TIMEOUT_MS 1000
#define DMA_CAPACITY_US 90000
#define DMA_DRAIN_US 120000
#define WORKER_STACK_BYTES 4096
#define WORKER_PRIORITY 5

struct mybot_pcm_playback_buffer {
    mybot_pcm_write_fn write;
    void *write_context;
    int16_t *fifo;
    int16_t scratch[WRITE_FRAMES];
    StaticSemaphore_t mutex_storage;
    StaticSemaphore_t wake_storage;
    StaticSemaphore_t space_storage;
    StaticSemaphore_t changed_storage;
    SemaphoreHandle_t mutex;
    SemaphoreHandle_t wake;
    SemaphoreHandle_t space;
    SemaphoreHandle_t changed;
    bool running;
    bool idle;
    bool terminate;
    bool exited;
    bool fault;
    bool prebuffer;
    bool draining;
    bool busy;
    size_t read_at;
    size_t write_at;
    size_t queued;
    size_t pending;
    size_t offset;
    int64_t first_data_us;
    int64_t dma_end_us;
    int64_t last_write_us;
    uint64_t admitted_frames;
    uint64_t dma_written_frames;
    uint32_t rebuffer_events;
    uint32_t queue_high_water;
    uint32_t driver_errors;
    uint32_t driver_timeouts;
    uint32_t producer_timeouts;
    uint32_t no_progress;
#if CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR
    int64_t previous_write_begin;
    int64_t previous_write_end;
#endif
};

static TickType_t wait_ticks(int64_t remaining_us) {
    if (remaining_us <= 0) {
        return 0;
    }
    TickType_t ticks = pdMS_TO_TICKS((uint32_t)((remaining_us + 999) / 1000));
    return ticks ? ticks : 1;
}

static void lock_buffer(mybot_pcm_playback_buffer_t *buffer) {
    (void)xSemaphoreTake(buffer->mutex, portMAX_DELAY);
}

static void unlock_buffer(mybot_pcm_playback_buffer_t *buffer) {
    (void)xSemaphoreGive(buffer->mutex);
}

/* The FIFO mutex is never held across I2S writes or waits. Only this worker
 * touches scratch; freeing FIFO space cannot overwrite a pending I2S write. */
static void playback_worker(void *opaque) {
    mybot_pcm_playback_buffer_t *buffer = opaque;
    for (;;) {
        TickType_t wait = portMAX_DELAY;
        bool copied = false;
        lock_buffer(buffer);
        if (buffer->terminate) {
            unlock_buffer(buffer);
            break;
        }
        if (!buffer->running) {
            buffer->queued = 0;
            buffer->pending = 0;
            buffer->offset = 0;
            buffer->read_at = 0;
            buffer->write_at = 0;
            buffer->busy = false;
            buffer->idle = true;
            unlock_buffer(buffer);
            (void)xSemaphoreGive(buffer->changed);
            (void)xSemaphoreTake(buffer->wake, portMAX_DELAY);
            continue;
        }
        buffer->idle = false;
        int64_t now = esp_timer_get_time();
        if (!buffer->prebuffer && buffer->pending == 0 && buffer->dma_end_us > 0 &&
            now >= buffer->dma_end_us) {
            /* Also rebuffer when a late packet arrived before this task woke. */
            buffer->prebuffer = true;
            if (buffer->queued == 0) {
                buffer->first_data_us = 0;
            }
            ++buffer->rebuffer_events;
        }
        if (buffer->prebuffer && buffer->queued > 0) {
            int64_t remaining = buffer->first_data_us + PREBUFFER_TIMEOUT_MS * 1000 - now;
            if (buffer->queued >= PREBUFFER_FRAMES || remaining <= 0 || buffer->draining) {
                buffer->prebuffer = false;
            } else {
                wait = wait_ticks(remaining);
            }
        }
        if (!buffer->prebuffer && buffer->pending == 0 && buffer->queued > 0) {
            size_t count = buffer->queued < WRITE_FRAMES ? buffer->queued : WRITE_FRAMES;
            size_t first = FIFO_FRAMES - buffer->read_at;
            if (first > count) {
                first = count;
            }
            memcpy(buffer->scratch, buffer->fifo + buffer->read_at,
                   first * sizeof(buffer->scratch[0]));
            memcpy(buffer->scratch + first, buffer->fifo,
                   (count - first) * sizeof(buffer->scratch[0]));
            buffer->read_at = (buffer->read_at + count) % FIFO_FRAMES;
            buffer->queued -= count;
            buffer->pending = count;
            buffer->offset = 0;
            buffer->busy = true;
            copied = true;
        }
        if (!buffer->prebuffer && buffer->pending == 0 && buffer->queued == 0) {
            /* A temporarily empty software FIFO is normal: DMA still holds
             * audio. Wait for the next producer packet before rebuffering.
             * This deadline is an estimate for pacing, not an underrun counter. */
            wait = wait_ticks(buffer->dma_end_us - now);
        }
        size_t pending = buffer->pending;
        size_t offset = buffer->offset;
        unlock_buffer(buffer);
        if (copied) {
            (void)xSemaphoreGive(buffer->space);
        }
        if (pending == 0) {
            (void)xSemaphoreTake(buffer->wake, wait);
            continue;
        }

        size_t frames = 0;
        int64_t begin = esp_timer_get_time();
        esp_err_t err = buffer->write(buffer->write_context, buffer->scratch + offset, pending,
                                      &frames, I2S_TIMEOUT_MS);
        int64_t end = esp_timer_get_time();
        bool invalid_count = frames > pending;
        if (invalid_count) {
            frames = 0;
        }
        bool failed = invalid_count || (err != ESP_OK && err != ESP_ERR_TIMEOUT);
#if CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR
        mybot_debug_record_playback((uint32_t)pending, (uint32_t)frames, err == ESP_ERR_TIMEOUT,
                                    failed, begin, end, buffer->previous_write_begin,
                                    buffer->previous_write_end);
        buffer->previous_write_begin = begin;
        buffer->previous_write_end = end;
#endif
        lock_buffer(buffer);
        buffer->pending -= frames;
        buffer->offset += frames;
        buffer->dma_written_frames += frames;
        if (frames > 0) {
            int64_t queued_until = buffer->dma_end_us > begin ? buffer->dma_end_us : begin;
            queued_until += (int64_t)frames * 1000000 / PCM_RATE;
            buffer->dma_end_us =
                queued_until < end + DMA_CAPACITY_US ? queued_until : end + DMA_CAPACITY_US;
            buffer->last_write_us = end;
            buffer->no_progress = 0;
        } else {
            ++buffer->no_progress;
            failed |= buffer->no_progress >= 3;
        }
        if (err == ESP_ERR_TIMEOUT) {
            ++buffer->driver_timeouts;
        }
        if (failed) {
            ++buffer->driver_errors;
            buffer->fault = true;
            buffer->running = false;
        }
        if (buffer->pending == 0) {
            buffer->busy = false;
        }
        unlock_buffer(buffer);
        (void)xSemaphoreGive(buffer->changed);
        if (failed) {
            ESP_LOGE(TAG,
                     "event=playback_buffer action=write result=error error=%s "
                     "invalid_count=%d",
                     esp_err_to_name(err), invalid_count);
            (void)xSemaphoreGive(buffer->space);
        } else if (frames == 0) {
            /* Defend against a driver returning immediately without progress. */
            vTaskDelay(1);
        }
    }

    /* This is the final context access. A dynamically allocated FreeRTOS task
     * owns its stack/TCB until deletion; destroy() only frees our separate data.
     * No completion semaphore can still be accessed after the release store. */
    __atomic_store_n(&buffer->exited, true, __ATOMIC_RELEASE);
    vTaskDelete(NULL);
}

static void release_buffer(mybot_pcm_playback_buffer_t *buffer) {
    if (buffer->changed) {
        vSemaphoreDelete(buffer->changed);
    }
    if (buffer->space) {
        vSemaphoreDelete(buffer->space);
    }
    if (buffer->wake) {
        vSemaphoreDelete(buffer->wake);
    }
    if (buffer->mutex) {
        vSemaphoreDelete(buffer->mutex);
    }
    free(buffer->fifo);
    free(buffer);
}

mybot_pcm_playback_buffer_t *mybot_pcm_playback_buffer_create(mybot_pcm_write_fn write,
                                                              void *context) {
    if (!write || !context) {
        return NULL;
    }
    mybot_pcm_playback_buffer_t *buffer =
        heap_caps_calloc(1, sizeof(*buffer), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buffer) {
        return NULL;
    }
    buffer->write = write;
    buffer->write_context = context;
    buffer->idle = true;
    buffer->fifo =
        heap_caps_calloc(FIFO_FRAMES, sizeof(buffer->fifo[0]), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer->fifo) {
        buffer->fifo = heap_caps_calloc(FIFO_FRAMES, sizeof(buffer->fifo[0]),
                                        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    buffer->mutex = xSemaphoreCreateMutexStatic(&buffer->mutex_storage);
    buffer->wake = xSemaphoreCreateBinaryStatic(&buffer->wake_storage);
    buffer->space = xSemaphoreCreateBinaryStatic(&buffer->space_storage);
    buffer->changed = xSemaphoreCreateBinaryStatic(&buffer->changed_storage);
    if (!buffer->fifo || !buffer->mutex || !buffer->wake || !buffer->space || !buffer->changed ||
        xTaskCreate(playback_worker, "pcm_playback", WORKER_STACK_BYTES, buffer, WORKER_PRIORITY,
                    NULL) != pdPASS) {
        release_buffer(buffer);
        return NULL;
    }
    return buffer;
}

int mybot_pcm_playback_buffer_start(mybot_pcm_playback_buffer_t *buffer) {
    if (!buffer) {
        return -1;
    }
    lock_buffer(buffer);
    if (buffer->running || !buffer->idle || buffer->terminate) {
        unlock_buffer(buffer);
        return -1;
    }
    buffer->queued = 0;
    buffer->pending = 0;
    buffer->offset = 0;
    buffer->read_at = 0;
    buffer->write_at = 0;
    buffer->fault = false;
    buffer->busy = false;
    buffer->prebuffer = true;
    buffer->draining = false;
    buffer->first_data_us = 0;
    buffer->dma_end_us = 0;
    buffer->last_write_us = 0;
    buffer->admitted_frames = 0;
    buffer->dma_written_frames = 0;
    buffer->rebuffer_events = 0;
    buffer->queue_high_water = 0;
    buffer->driver_errors = 0;
    buffer->driver_timeouts = 0;
    buffer->producer_timeouts = 0;
    buffer->no_progress = 0;
#if CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR
    buffer->previous_write_begin = 0;
    buffer->previous_write_end = 0;
#endif
    buffer->idle = false;
    buffer->running = true;
    unlock_buffer(buffer);
    (void)xSemaphoreGive(buffer->wake);
    ESP_LOGI(TAG,
             "event=playback_buffer action=start result=ok capacity_frames=%d "
             "prebuffer_frames=%d write_frames=%d",
             FIFO_FRAMES, PREBUFFER_FRAMES, WRITE_FRAMES);
    return 0;
}

int mybot_pcm_playback_buffer_write(mybot_pcm_playback_buffer_t *buffer, const void *pcm,
                                    int frames) {
    if (!buffer || !pcm || frames <= 0) {
        return -1;
    }
    const int16_t *source = pcm;
    size_t accepted = 0;
    int64_t deadline = esp_timer_get_time() + PRODUCER_TIMEOUT_MS * 1000;
    while (accepted < (size_t)frames) {
        lock_buffer(buffer);
        if (!buffer->running || buffer->fault) {
            bool failed = buffer->fault;
            unlock_buffer(buffer);
            return accepted > 0 ? (int)accepted : (failed ? -1 : 0);
        }
        size_t count = FIFO_FRAMES - buffer->queued;
        if (count > (size_t)frames - accepted) {
            count = (size_t)frames - accepted;
        }
        if (count > 0) {
            size_t first = FIFO_FRAMES - buffer->write_at;
            if (first > count) {
                first = count;
            }
            memcpy(buffer->fifo + buffer->write_at, source + accepted,
                   first * sizeof(buffer->fifo[0]));
            memcpy(buffer->fifo, source + accepted + first,
                   (count - first) * sizeof(buffer->fifo[0]));
            if (buffer->queued == 0) {
                buffer->first_data_us = esp_timer_get_time();
            }
            buffer->write_at = (buffer->write_at + count) % FIFO_FRAMES;
            buffer->queued += count;
            buffer->admitted_frames += count;
            if (buffer->queued > buffer->queue_high_water) {
                buffer->queue_high_water = (uint32_t)buffer->queued;
            }
            accepted += count;
        }
        int64_t remaining = deadline - esp_timer_get_time();
        if (accepted < (size_t)frames && remaining <= 0) {
            ++buffer->producer_timeouts;
        }
        unlock_buffer(buffer);
        if (count > 0) {
            (void)xSemaphoreGive(buffer->wake);
        }
        if (accepted == (size_t)frames || remaining <= 0) {
            break;
        }
        (void)xSemaphoreTake(buffer->space, wait_ticks(remaining));
    }
    return (int)accepted;
}

int mybot_pcm_playback_buffer_stop(mybot_pcm_playback_buffer_t *buffer) {
    if (!buffer) {
        return -1;
    }
    int64_t deadline = esp_timer_get_time() + STOP_TIMEOUT_MS * 1000;
    lock_buffer(buffer);
    buffer->running = false;
    buffer->draining = false;
    unlock_buffer(buffer);
    (void)xSemaphoreGive(buffer->wake);
    (void)xSemaphoreGive(buffer->space);
    for (;;) {
        lock_buffer(buffer);
        bool idle = buffer->idle;
        unlock_buffer(buffer);
        if (idle) {
            break;
        }
        int64_t remaining = deadline - esp_timer_get_time();
        if (remaining <= 0) {
            ESP_LOGE(TAG, "event=playback_buffer action=stop result=timeout resources=retained");
            return -1;
        }
        (void)xSemaphoreTake(buffer->changed, wait_ticks(remaining));
    }
    ESP_LOGI(TAG,
             "event=playback_buffer action=stop result=ok admitted_frames=%" PRIu64
             " dma_written_frames=%" PRIu64 " rebuffer_events=%" PRIu32 " queue_high_water=%" PRIu32
             " driver_errors=%" PRIu32 " driver_timeouts=%" PRIu32 " producer_timeouts=%" PRIu32,
             buffer->admitted_frames, buffer->dma_written_frames, buffer->rebuffer_events,
             buffer->queue_high_water, buffer->driver_errors, buffer->driver_timeouts,
             buffer->producer_timeouts);
    return 0;
}

int mybot_pcm_playback_buffer_drain(mybot_pcm_playback_buffer_t *buffer, uint32_t timeout_ms) {
    if (!buffer) {
        return -1;
    }
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    lock_buffer(buffer);
    buffer->draining = true;
    unlock_buffer(buffer);
    (void)xSemaphoreGive(buffer->wake);
    int result = -1;
    for (;;) {
        lock_buffer(buffer);
        bool failed = buffer->fault;
        bool stopped = !buffer->running;
        bool empty = buffer->queued == 0 && !buffer->busy;
        int64_t last_write = buffer->last_write_us;
        unlock_buffer(buffer);
        int64_t now = esp_timer_get_time();
        if (failed) {
            break;
        }
        if (stopped || (empty && (last_write == 0 || now >= last_write + DMA_DRAIN_US))) {
            result = 0;
            break;
        }
        if (now >= deadline) {
            break;
        }
        int64_t wait = deadline - now;
        if (empty && last_write + DMA_DRAIN_US - now < wait) {
            wait = last_write + DMA_DRAIN_US - now;
        }
        (void)xSemaphoreTake(buffer->changed, wait_ticks(wait));
    }
    lock_buffer(buffer);
    buffer->draining = false;
    unlock_buffer(buffer);
    return result;
}

int mybot_pcm_playback_buffer_destroy(mybot_pcm_playback_buffer_t *buffer) {
    if (!buffer) {
        return 0;
    }
    if (!__atomic_load_n(&buffer->exited, __ATOMIC_ACQUIRE)) {
        lock_buffer(buffer);
        bool idle = buffer->idle;
        unlock_buffer(buffer);
        if (!idle && mybot_pcm_playback_buffer_stop(buffer) < 0) {
            return -1;
        }
        lock_buffer(buffer);
        buffer->terminate = true;
        unlock_buffer(buffer);
        (void)xSemaphoreGive(buffer->wake);
        int64_t deadline = esp_timer_get_time() + STOP_TIMEOUT_MS * 1000;
        while (!__atomic_load_n(&buffer->exited, __ATOMIC_ACQUIRE)) {
            if (esp_timer_get_time() >= deadline) {
                ESP_LOGE(TAG, "event=playback_buffer action=destroy result=timeout "
                              "resources=retained");
                return -1;
            }
            vTaskDelay(1);
        }
    }
    release_buffer(buffer);
    return 0;
}
