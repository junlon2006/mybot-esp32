/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#include "board_config.h"

#include <mybot/platform/mybot_audio.h>
#include <api/aosl_atomic.h>

#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define AUDIO_CONTEXT_MAGIC 0x58564641U
#define AUDIO_IO_TIMEOUT_MS 50
#define AUDIO_MAX_FRAMES 960
#define AUDIO_WIRE_CHANNELS 2
#define AUDIO_VOLUME_GAIN_Q16_ONE 65536
#define AUDIO_VOLUME_NVS_KEY "output_volume"
#define AUDIO_VOLUME_NVS_NAMESPACE "audio"
#define TAG "xvf3800_audio"

#if MYBOT_AUDIO_CAPTURE_CHANNEL < 0 || MYBOT_AUDIO_CAPTURE_CHANNEL >= AUDIO_WIRE_CHANNELS
#error "MYBOT_AUDIO_CAPTURE_CHANNEL must select I2S channel 0 or 1"
#endif

#if MYBOT_AUDIO_INPUT_SHIFT_BITS < 0 || MYBOT_AUDIO_INPUT_SHIFT_BITS > 31
#error "MYBOT_AUDIO_INPUT_SHIFT_BITS must be between 0 and 31"
#endif

#if MYBOT_AUDIO_DEFAULT_VOLUME < MYBOT_AUDIO_VOLUME_MIN ||                                         \
    MYBOT_AUDIO_DEFAULT_VOLUME > MYBOT_AUDIO_VOLUME_MAX
#error "MYBOT_AUDIO_DEFAULT_VOLUME must be between 0 and 100"
#endif

typedef enum {
    AUDIO_DIRECTION_CAPTURE,
    AUDIO_DIRECTION_PLAYBACK,
} audio_direction_t;

typedef struct {
    uint32_t magic;
    audio_direction_t direction;
    aosl_atomic_t started;
    int32_t *scratch;
} audio_stream_context_t;

typedef struct {
    i2s_chan_handle_t tx_channel;
    i2s_chan_handle_t rx_channel;
    unsigned references;
    bool ready;
    bool capture_attached;
    bool playback_attached;
    bool capture_started;
    bool playback_started;
    bool rx_enabled;
    bool tx_enabled;
} xvf3800_audio_shared_t;

typedef struct {
    nvs_handle_t handle;
    bool active;
    bool persisted_volume_known;
    bool persist_dirty;
    int persisted_volume;
} audio_volume_context_t;

static xvf3800_audio_shared_t s_audio;
static audio_volume_context_t s_volume_context;
static aosl_atomic_t s_output_volume = MYBOT_AUDIO_DEFAULT_VOLUME;
static aosl_atomic_t s_output_gain_q16 =
    (MYBOT_AUDIO_DEFAULT_VOLUME * MYBOT_AUDIO_DEFAULT_VOLUME * AUDIO_VOLUME_GAIN_Q16_ONE) /
    (MYBOT_AUDIO_VOLUME_MAX * MYBOT_AUDIO_VOLUME_MAX);
static StaticSemaphore_t s_audio_mutex_storage;
static SemaphoreHandle_t s_audio_mutex;
static portMUX_TYPE s_audio_mutex_init_lock = portMUX_INITIALIZER_UNLOCKED;

static SemaphoreHandle_t audio_mutex_get(void) {
    portENTER_CRITICAL(&s_audio_mutex_init_lock);
    if (!s_audio_mutex) {
        s_audio_mutex = xSemaphoreCreateMutexStatic(&s_audio_mutex_storage);
    }
    SemaphoreHandle_t mutex = s_audio_mutex;
    portEXIT_CRITICAL(&s_audio_mutex_init_lock);
    return mutex;
}

static bool audio_lock(void) {
    SemaphoreHandle_t mutex = audio_mutex_get();
    return mutex && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE;
}

static void audio_unlock(void) {
    xSemaphoreGive(s_audio_mutex);
}

static const char *direction_name(audio_direction_t direction) {
    return direction == AUDIO_DIRECTION_CAPTURE ? "capture" : "playback";
}

static int validate_format(int rate, int channels, int bits) {
    return rate == MYBOT_AUDIO_SAMPLE_RATE && channels == 1 && bits == 16 ? 0 : -1;
}

static int disable_channel(i2s_chan_handle_t channel, bool *enabled, const char *direction) {
    if (!channel || !*enabled) {
        return 0;
    }

    esp_err_t err = i2s_channel_disable(channel);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "event=audio_device direction=%s action=disable result=error error=%s",
                 direction, esp_err_to_name(err));
        return -1;
    }
    *enabled = false;
    return 0;
}

static int release_channel(i2s_chan_handle_t *channel, bool *enabled, const char *direction) {
    if (!*channel) {
        return 0;
    }
    if (disable_channel(*channel, enabled, direction) < 0) {
        return -1;
    }

    esp_err_t err = i2s_del_channel(*channel);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "event=audio_device direction=%s action=destroy result=error error=%s",
                 direction, esp_err_to_name(err));
        return -1;
    }
    *channel = NULL;
    return 0;
}

static int shared_release_locked(void) {
    int result = 0;
    if (release_channel(&s_audio.rx_channel, &s_audio.rx_enabled, "capture") < 0) {
        result = -1;
    }
    if (release_channel(&s_audio.tx_channel, &s_audio.tx_enabled, "playback") < 0) {
        result = -1;
    }

    s_audio.references = 0;
    s_audio.ready = false;
    s_audio.capture_attached = false;
    s_audio.playback_attached = false;
    s_audio.capture_started = false;
    s_audio.playback_started = false;
    if (!s_audio.rx_channel && !s_audio.tx_channel) {
        s_audio = (xvf3800_audio_shared_t){0};
    }
    return result;
}

static int initialize_i2s_locked(void) {
    if (MYBOT_AUDIO_CAPTURE_CHANNEL != 0 && MYBOT_AUDIO_CAPTURE_CHANNEL != 1) {
        ESP_LOGE(TAG,
                 "event=audio_device direction=capture action=initialize result=error "
                 "reason=capture_channel value=%d",
                 MYBOT_AUDIO_CAPTURE_CHANNEL);
        return -1;
    }

    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);
    channel_config.dma_desc_num = 6;
    channel_config.dma_frame_num = 240;
    channel_config.auto_clear_after_cb = true;

    esp_err_t err = i2s_new_channel(&channel_config, &s_audio.tx_channel, &s_audio.rx_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "event=audio_device action=initialize result=error reason=i2s_channel error=%s",
                 esp_err_to_name(err));
        return -1;
    }

    i2s_std_config_t duplex_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(MYBOT_AUDIO_SAMPLE_RATE),
        .slot_cfg =
            {
                .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                .slot_mode = I2S_SLOT_MODE_STEREO,
                .slot_mask = I2S_STD_SLOT_BOTH,
                .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
                .ws_pol = false,
                .bit_shift = true,
                .left_align = false,
                .big_endian = false,
                .bit_order_lsb = false,
            },
        .gpio_cfg =
            {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = MYBOT_AUDIO_I2S_BCLK,
                .ws = MYBOT_AUDIO_I2S_WS,
                .dout = MYBOT_AUDIO_I2S_DOUT,
                .din = MYBOT_AUDIO_I2S_DIN,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
            },
    };

    err = i2s_channel_init_std_mode(s_audio.tx_channel, &duplex_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "event=audio_device direction=playback action=initialize result=error "
                 "reason=i2s_mode error=%s",
                 esp_err_to_name(err));
        return -1;
    }
    err = i2s_channel_init_std_mode(s_audio.rx_channel, &duplex_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "event=audio_device direction=capture action=initialize result=error "
                 "reason=i2s_mode error=%s",
                 esp_err_to_name(err));
        return -1;
    }
    return 0;
}

static int shared_acquire_locked(audio_direction_t direction) {
    bool *attached = direction == AUDIO_DIRECTION_CAPTURE ? &s_audio.capture_attached
                                                          : &s_audio.playback_attached;
    if (*attached) {
        ESP_LOGE(TAG,
                 "event=audio_device direction=%s action=initialize result=error "
                 "reason=already_attached",
                 direction_name(direction));
        return -1;
    }

    if (!s_audio.ready && (s_audio.tx_channel || s_audio.rx_channel) &&
        shared_release_locked() < 0) {
        ESP_LOGE(TAG,
                 "event=audio_device direction=%s action=initialize result=error "
                 "reason=cleanup_pending",
                 direction_name(direction));
        return -1;
    }

    if (!s_audio.ready) {
        if (initialize_i2s_locked() < 0) {
            (void)shared_release_locked();
            return -1;
        }
        s_audio.ready = true;
        ESP_LOGI(TAG,
                 "event=audio_device action=initialize rate=%d role=slave bus=stereo32 "
                 "capture_channel=%d input_shift=%d duplex=1 result=ok",
                 MYBOT_AUDIO_SAMPLE_RATE, MYBOT_AUDIO_CAPTURE_CHANNEL,
                 MYBOT_AUDIO_INPUT_SHIFT_BITS);
    }

    *attached = true;
    ++s_audio.references;
    return 0;
}

static int stream_init(void **out_context, int rate, int channels, int bits,
                       audio_direction_t direction) {
    ESP_LOGI(TAG, "event=sdk_adapter adapter=audio direction=%s action=initialize",
             direction_name(direction));
    if (!out_context || validate_format(rate, channels, bits) < 0) {
        return -1;
    }
    *out_context = NULL;

    audio_stream_context_t *context = calloc(1, sizeof(*context));
    if (!context) {
        return -1;
    }
    context->scratch = calloc(AUDIO_MAX_FRAMES * AUDIO_WIRE_CHANNELS, sizeof(*context->scratch));
    if (!context->scratch) {
        free(context);
        return -1;
    }
    context->magic = AUDIO_CONTEXT_MAGIC;
    context->direction = direction;

    if (!audio_lock()) {
        free(context->scratch);
        free(context);
        return -1;
    }
    int result = shared_acquire_locked(direction);
    audio_unlock();
    if (result < 0) {
        free(context->scratch);
        free(context);
        return -1;
    }

    *out_context = context;
    return 0;
}

static int capture_init(void **out_context, int rate, int channels, int bits) {
    return stream_init(out_context, rate, channels, bits, AUDIO_DIRECTION_CAPTURE);
}

static int playback_init(void **out_context, int rate, int channels, int bits) {
    return stream_init(out_context, rate, channels, bits, AUDIO_DIRECTION_PLAYBACK);
}

static int enable_tx_locked(void) {
    if (s_audio.tx_enabled) {
        return 0;
    }

    esp_err_t err = i2s_channel_enable(s_audio.tx_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "event=audio_device direction=playback action=enable result=error error=%s",
                 esp_err_to_name(err));
        return -1;
    }
    s_audio.tx_enabled = true;
    return 0;
}

static int capture_start(void *opaque) {
    audio_stream_context_t *context = opaque;
    ESP_LOGI(TAG, "event=sdk_adapter adapter=audio direction=capture action=start");
    if (!context || context->magic != AUDIO_CONTEXT_MAGIC ||
        context->direction != AUDIO_DIRECTION_CAPTURE || aosl_atomic_read(&context->started) ||
        !audio_lock()) {
        return -1;
    }

    int result = -1;
    if (!s_audio.ready || s_audio.capture_started || enable_tx_locked() < 0) {
        goto done;
    }

    esp_err_t err = i2s_channel_enable(s_audio.rx_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "event=audio_device direction=capture action=enable result=error error=%s",
                 esp_err_to_name(err));
        if (!s_audio.playback_started) {
            (void)disable_channel(s_audio.tx_channel, &s_audio.tx_enabled, "playback");
        }
        goto done;
    }

    s_audio.rx_enabled = true;
    s_audio.capture_started = true;
    aosl_atomic_set(&context->started, true);
    result = 0;
done:
    audio_unlock();
    return result;
}

static int playback_start(void *opaque) {
    audio_stream_context_t *context = opaque;
    ESP_LOGI(TAG, "event=sdk_adapter adapter=audio direction=playback action=start");
    if (!context || context->magic != AUDIO_CONTEXT_MAGIC ||
        context->direction != AUDIO_DIRECTION_PLAYBACK || aosl_atomic_read(&context->started) ||
        !audio_lock()) {
        return -1;
    }

    int result = -1;
    if (!s_audio.ready || s_audio.playback_started || enable_tx_locked() < 0) {
        goto done;
    }

    s_audio.playback_started = true;
    aosl_atomic_set(&context->started, true);
    result = 0;
done:
    audio_unlock();
    return result;
}

static int capture_read(void *opaque, void *buffer, int frames) {
    audio_stream_context_t *context = opaque;
    if (!context || context->magic != AUDIO_CONTEXT_MAGIC ||
        context->direction != AUDIO_DIRECTION_CAPTURE || !aosl_atomic_read(&context->started) ||
        !buffer || frames <= 0 || frames > AUDIO_MAX_FRAMES) {
        return -1;
    }

    size_t bytes_read = 0;
    esp_err_t err =
        i2s_channel_read(s_audio.rx_channel, context->scratch,
                         (size_t)frames * AUDIO_WIRE_CHANNELS * sizeof(context->scratch[0]),
                         &bytes_read, AUDIO_IO_TIMEOUT_MS);
    if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
        return aosl_atomic_read(&context->started) ? -1 : 0;
    }

    int captured_frames = (int)(bytes_read / (AUDIO_WIRE_CHANNELS * sizeof(context->scratch[0])));
    int16_t *output = buffer;
    for (int frame = 0; frame < captured_frames; ++frame) {
        int32_t sample =
            context->scratch[frame * AUDIO_WIRE_CHANNELS + MYBOT_AUDIO_CAPTURE_CHANNEL] >>
            MYBOT_AUDIO_INPUT_SHIFT_BITS;
        if (sample > INT16_MAX) {
            sample = INT16_MAX;
        } else if (sample < INT16_MIN) {
            sample = INT16_MIN;
        }
        output[frame] = (int16_t)sample;
    }
    return captured_frames;
}

static int playback_write(void *opaque, const void *buffer, int frames) {
    audio_stream_context_t *context = opaque;
    if (!context || context->magic != AUDIO_CONTEXT_MAGIC ||
        context->direction != AUDIO_DIRECTION_PLAYBACK || !aosl_atomic_read(&context->started) ||
        !buffer || frames <= 0 || frames > AUDIO_MAX_FRAMES) {
        return -1;
    }

    const int16_t *input = buffer;
    int gain_q16 = (int)aosl_atomic_read(&s_output_gain_q16);
    for (int frame = 0; frame < frames; ++frame) {
        int32_t scaled = (int32_t)(((int64_t)input[frame] * gain_q16) >> 16);
        int32_t wire_sample = scaled * AUDIO_VOLUME_GAIN_Q16_ONE;
        context->scratch[frame * AUDIO_WIRE_CHANNELS] = wire_sample;
        context->scratch[frame * AUDIO_WIRE_CHANNELS + 1] = wire_sample;
    }

    size_t bytes_written = 0;
    esp_err_t err =
        i2s_channel_write(s_audio.tx_channel, context->scratch,
                          (size_t)frames * AUDIO_WIRE_CHANNELS * sizeof(context->scratch[0]),
                          &bytes_written, AUDIO_IO_TIMEOUT_MS);
    if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
        return aosl_atomic_read(&context->started) ? -1 : 0;
    }
    return (int)(bytes_written / (AUDIO_WIRE_CHANNELS * sizeof(context->scratch[0])));
}

static int stream_stop(void *opaque) {
    audio_stream_context_t *context = opaque;
    if (!context || context->magic != AUDIO_CONTEXT_MAGIC || !audio_lock()) {
        return -1;
    }
    if (!aosl_atomic_read(&context->started)) {
        audio_unlock();
        return 0;
    }

    const char *direction = direction_name(context->direction);
    ESP_LOGI(TAG, "event=sdk_adapter adapter=audio direction=%s action=stop", direction);
    aosl_atomic_set(&context->started, false);

    int result = 0;
    if (context->direction == AUDIO_DIRECTION_CAPTURE) {
        s_audio.capture_started = false;
        if (disable_channel(s_audio.rx_channel, &s_audio.rx_enabled, "capture") < 0) {
            result = -1;
        }
        if (!s_audio.playback_started &&
            disable_channel(s_audio.tx_channel, &s_audio.tx_enabled, "playback") < 0) {
            result = -1;
        }
    } else {
        s_audio.playback_started = false;
        if (!s_audio.capture_started &&
            disable_channel(s_audio.tx_channel, &s_audio.tx_enabled, "playback") < 0) {
            result = -1;
        }
    }
    audio_unlock();
    return result;
}

static void stream_destroy(void *opaque) {
    audio_stream_context_t *context = opaque;
    if (!context || context->magic != AUDIO_CONTEXT_MAGIC) {
        return;
    }

    (void)stream_stop(context);
    ESP_LOGI(TAG, "event=sdk_adapter adapter=audio direction=%s action=destroy",
             direction_name(context->direction));

    if (audio_lock()) {
        bool *attached = context->direction == AUDIO_DIRECTION_CAPTURE ? &s_audio.capture_attached
                                                                       : &s_audio.playback_attached;
        *attached = false;
        if (s_audio.references > 0) {
            --s_audio.references;
        }
        if (s_audio.references == 0) {
            int result = shared_release_locked();
            ESP_LOGI(TAG, "event=audio_device action=destroy result=%s",
                     result == 0 ? "ok" : "error");
        }
        audio_unlock();
    }

    context->magic = 0;
    free(context->scratch);
    free(context);
}

static int volume_to_gain_q16(int volume) {
    return (int)(((int64_t)volume * volume * AUDIO_VOLUME_GAIN_Q16_ONE) /
                 (MYBOT_AUDIO_VOLUME_MAX * MYBOT_AUDIO_VOLUME_MAX));
}

static void apply_output_volume(int volume) {
    aosl_atomic_set(&s_output_volume, volume);
    aosl_atomic_set(&s_output_gain_q16, volume_to_gain_q16(volume));
}

static int persist_volume(audio_volume_context_t *context, int volume) {
    if (nvs_set_i32(context->handle, AUDIO_VOLUME_NVS_KEY, volume) != ESP_OK ||
        nvs_commit(context->handle) != ESP_OK) {
        return -1;
    }
    context->persisted_volume = volume;
    context->persisted_volume_known = true;
    context->persist_dirty = false;
    return 0;
}

static bool erase_invalid_volume(nvs_handle_t handle) {
    esp_err_t err = nvs_erase_key(handle, AUDIO_VOLUME_NVS_KEY);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "event=volume action=erase_invalid result=error reason=erase error=%s",
                 esp_err_to_name(err));
        return false;
    }
    if (err == ESP_OK && nvs_commit(handle) != ESP_OK) {
        ESP_LOGW(TAG, "event=volume action=erase_invalid result=error reason=commit");
        return false;
    }
    return true;
}

static int volume_init(void **out_context) {
    if (!out_context || s_volume_context.active) {
        return -1;
    }
    *out_context = NULL;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(AUDIO_VOLUME_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "event=volume action=restore result=error reason=nvs_open error=%s",
                 esp_err_to_name(err));
        return -1;
    }

    int volume = MYBOT_AUDIO_DEFAULT_VOLUME;
    const char *source = "default";
    bool persist_dirty = false;
    int32_t stored_volume = 0;
    err = nvs_get_i32(handle, AUDIO_VOLUME_NVS_KEY, &stored_volume);
    bool stored_volume_valid = err == ESP_OK && stored_volume >= MYBOT_AUDIO_VOLUME_MIN &&
                               stored_volume <= MYBOT_AUDIO_VOLUME_MAX;
    if (stored_volume_valid) {
        volume = (int)stored_volume;
        source = "nvs";
    } else if (err == ESP_OK || err == ESP_ERR_NVS_TYPE_MISMATCH) {
        ESP_LOGW(TAG, "event=volume action=restore result=ignored reason=invalid_record");
        persist_dirty = !erase_invalid_volume(handle);
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "event=volume action=restore result=ignored reason=nvs_read error=%s",
                 esp_err_to_name(err));
    }

    s_volume_context = (audio_volume_context_t){
        .handle = handle,
        .active = true,
        .persisted_volume_known = stored_volume_valid,
        .persist_dirty = persist_dirty,
        .persisted_volume = volume,
    };
    apply_output_volume(volume);
    *out_context = &s_volume_context;
    ESP_LOGI(TAG, "event=volume action=restore value=%d source=%s result=ok", volume, source);
    return 0;
}

static int volume_set(void *opaque, int volume) {
    audio_volume_context_t *context = opaque;
    if (context != &s_volume_context || !context->active || volume < MYBOT_AUDIO_VOLUME_MIN ||
        volume > MYBOT_AUDIO_VOLUME_MAX) {
        return -1;
    }

    int current_volume = (int)aosl_atomic_read(&s_output_volume);
    if (current_volume == volume && !context->persist_dirty && context->persisted_volume_known &&
        context->persisted_volume == volume) {
        return 0;
    }

    apply_output_volume(volume);
    if (!context->persist_dirty && context->persisted_volume_known &&
        context->persisted_volume == volume) {
        ESP_LOGI(TAG, "event=volume action=set value=%d persisted=1 result=ok", volume);
        return 0;
    }
    if (persist_volume(context, volume) < 0) {
        context->persist_dirty = true;
        ESP_LOGW(TAG,
                 "event=volume action=set value=%d applied=1 persisted=0 "
                 "result=error scope=persistence",
                 volume);
        return 0;
    }

    ESP_LOGI(TAG, "event=volume action=set value=%d persisted=1 result=ok", volume);
    return 0;
}

static int volume_get(void *opaque, int *volume) {
    audio_volume_context_t *context = opaque;
    if (context != &s_volume_context || !context->active || !volume) {
        return -1;
    }
    *volume = (int)aosl_atomic_read(&s_output_volume);
    return 0;
}

static void volume_destroy(void *opaque) {
    audio_volume_context_t *context = opaque;
    if (context != &s_volume_context || !context->active) {
        return;
    }

    int volume = (int)aosl_atomic_read(&s_output_volume);
    bool persisted = context->persisted_volume_known && context->persisted_volume == volume;
    if (context->persist_dirty) {
        persisted = persist_volume(context, volume) == 0;
    }
    if (context->persist_dirty) {
        ESP_LOGW(TAG,
                 "event=volume action=detach value=%d persisted=0 result=error "
                 "scope=persistence",
                 volume);
    } else {
        ESP_LOGI(TAG, "event=volume action=detach value=%d persisted=%d result=ok", volume,
                 persisted ? 1 : 0);
    }
    nvs_close(context->handle);
    *context = (audio_volume_context_t){0};
}

static const mybot_audio_capture_ops_t s_capture_ops = {
    .init = capture_init,
    .start = capture_start,
    .read = capture_read,
    .stop = stream_stop,
    .destroy = stream_destroy,
};

static const mybot_audio_playback_ops_t s_playback_ops = {
    .init = playback_init,
    .start = playback_start,
    .write = playback_write,
    .stop = stream_stop,
    .destroy = stream_destroy,
};

static const mybot_audio_volume_ops_t s_volume_ops = {
    .init = volume_init,
    .set_volume = volume_set,
    .get_volume = volume_get,
    .destroy = volume_destroy,
};

const mybot_audio_capture_ops_t *mybot_xvf3800_audio_capture_ops(void) {
    return &s_capture_ops;
}

const mybot_audio_playback_ops_t *mybot_xvf3800_audio_playback_ops(void) {
    return &s_playback_ops;
}

const mybot_audio_volume_ops_t *mybot_xvf3800_audio_volume_ops(void) {
    return &s_volume_ops;
}
