/* SPDX-License-Identifier: Apache-2.0 */
#include "board_config.h"

#include <mybot/platform/mybot_audio.h>
#include <api/aosl_atomic.h>

#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define AUDIO_MAX_FRAMES 960
#define AUDIO_IO_TIMEOUT_MS 50
#define AUDIO_VOLUME_DEFAULT 70
#define AUDIO_VOLUME_GAIN_Q16_ONE 65536
#define AUDIO_VOLUME_NVS_KEY "output_volume"
#define AUDIO_VOLUME_NVS_NAMESPACE "audio"
#define TAG "mybot_audio"

typedef struct {
    i2s_chan_handle_t channel;
    aosl_atomic_t started;
    const char *direction;
    int32_t scratch[AUDIO_MAX_FRAMES];
} audio_context_t;

typedef struct {
    nvs_handle_t handle;
    bool active;
    bool persisted_volume_known;
    bool persist_dirty;
    int persisted_volume;
} audio_volume_context_t;

static audio_volume_context_t s_volume_context;
static aosl_atomic_t s_output_volume;
static aosl_atomic_t s_output_gain_q16;

static int volume_to_gain_q16(int volume) {
    return (int)(((int64_t)volume * volume * AUDIO_VOLUME_GAIN_Q16_ONE) /
                 (MYBOT_AUDIO_VOLUME_MAX * MYBOT_AUDIO_VOLUME_MAX));
}

static void apply_output_volume(int volume) {
    aosl_atomic_set(&s_output_volume, volume);
    aosl_atomic_set(&s_output_gain_q16, volume_to_gain_q16(volume));
}

static int persist_volume(audio_volume_context_t *ctx, int volume) {
    if (nvs_set_i32(ctx->handle, AUDIO_VOLUME_NVS_KEY, volume) != ESP_OK ||
        nvs_commit(ctx->handle) != ESP_OK) {
        return -1;
    }
    ctx->persisted_volume = volume;
    ctx->persisted_volume_known = true;
    ctx->persist_dirty = false;
    return 0;
}

static void erase_invalid_volume(nvs_handle_t handle) {
    esp_err_t err = nvs_erase_key(handle, AUDIO_VOLUME_NVS_KEY);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "event=volume action=erase_invalid result=error reason=erase error=%s",
                 esp_err_to_name(err));
    } else if (err == ESP_OK && nvs_commit(handle) != ESP_OK) {
        ESP_LOGW(TAG, "event=volume action=erase_invalid result=error reason=commit");
    }
}

static int volume_init(void **out_ctx) {
    if (!out_ctx || s_volume_context.active) {
        return -1;
    }
    *out_ctx = NULL;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(AUDIO_VOLUME_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "event=volume action=restore result=error reason=nvs_open error=%s",
                 esp_err_to_name(err));
        return -1;
    }

    int volume = AUDIO_VOLUME_DEFAULT;
    const char *source = "default";
    int32_t stored_volume = 0;
    err = nvs_get_i32(handle, AUDIO_VOLUME_NVS_KEY, &stored_volume);
    bool stored_volume_valid = err == ESP_OK && stored_volume >= MYBOT_AUDIO_VOLUME_MIN &&
                               stored_volume <= MYBOT_AUDIO_VOLUME_MAX;
    if (stored_volume_valid) {
        volume = (int)stored_volume;
        source = "nvs";
    } else if (err == ESP_OK || err == ESP_ERR_NVS_TYPE_MISMATCH) {
        ESP_LOGW(TAG, "event=volume action=restore result=ignored reason=invalid_record");
        erase_invalid_volume(handle);
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "event=volume action=restore result=ignored reason=nvs_read error=%s",
                 esp_err_to_name(err));
    }

    s_volume_context = (audio_volume_context_t){
        .handle = handle,
        .active = true,
        .persisted_volume_known = stored_volume_valid,
        .persisted_volume = volume,
    };
    apply_output_volume(volume);
    *out_ctx = &s_volume_context;
    ESP_LOGI(TAG, "event=volume action=restore value=%d source=%s result=ok", volume, source);
    return 0;
}

static int volume_set(void *opaque, int volume) {
    audio_volume_context_t *ctx = opaque;
    if (ctx != &s_volume_context || !ctx->active || volume < MYBOT_AUDIO_VOLUME_MIN ||
        volume > MYBOT_AUDIO_VOLUME_MAX) {
        return -1;
    }

    int current_volume = (int)aosl_atomic_read(&s_output_volume);
    if (current_volume == volume && !ctx->persist_dirty && ctx->persisted_volume_known &&
        ctx->persisted_volume == volume) {
        return 0;
    }

    apply_output_volume(volume);
    if (!ctx->persist_dirty && ctx->persisted_volume_known && ctx->persisted_volume == volume) {
        ESP_LOGI(TAG, "event=volume action=set value=%d persisted=1 result=ok", volume);
        return 0;
    }
    if (persist_volume(ctx, volume) < 0) {
        ctx->persist_dirty = true;
        ESP_LOGW(TAG, "event=volume action=set value=%d applied=1 persisted=0 "
                      "result=error scope=persistence");
        return 0;
    }

    ESP_LOGI(TAG, "event=volume action=set value=%d persisted=1 result=ok", volume);
    return 0;
}

static int volume_get(void *opaque, int *volume) {
    audio_volume_context_t *ctx = opaque;
    if (ctx != &s_volume_context || !ctx->active || !volume) {
        return -1;
    }
    *volume = (int)aosl_atomic_read(&s_output_volume);
    return 0;
}

static void volume_destroy(void *opaque) {
    audio_volume_context_t *ctx = opaque;
    if (ctx != &s_volume_context || !ctx->active) {
        return;
    }

    int volume = (int)aosl_atomic_read(&s_output_volume);
    bool persisted = ctx->persisted_volume_known && ctx->persisted_volume == volume;
    if (ctx->persist_dirty) {
        persisted = persist_volume(ctx, volume) == 0;
    }
    if (ctx->persist_dirty) {
        ESP_LOGW(TAG, "event=volume action=detach value=%d persisted=0 result=error "
                      "scope=persistence");
    } else {
        ESP_LOGI(TAG, "event=volume action=detach value=%d persisted=%d result=ok", volume,
                 persisted ? 1 : 0);
    }
    nvs_close(ctx->handle);
    *ctx = (audio_volume_context_t){0};
}

static int validate_format(int rate, int channels, int bits) {
    return rate == MYBOT_AUDIO_SAMPLE_RATE && channels == 1 && bits == 16 ? 0 : -1;
}

static i2s_std_config_t make_i2s_config(int rate, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout,
                                        gpio_num_t din) {
    i2s_std_config_t config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg =
            {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = bclk,
                .ws = ws,
                .dout = dout,
                .din = din,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
            },
    };
    config.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    config.slot_cfg.ws_width = I2S_DATA_BIT_WIDTH_32BIT;
    config.slot_cfg.bit_shift = true;
    config.slot_cfg.left_align = true;
    return config;
}

static audio_context_t *audio_context_create(const char *direction) {
    audio_context_t *ctx = calloc(1, sizeof(audio_context_t));
    if (ctx) {
        ctx->direction = direction;
    }
    return ctx;
}

static int capture_init(void **out_ctx, int rate, int channels, int bits) {
    ESP_LOGI(TAG, "event=sdk_adapter adapter=audio direction=capture action=initialize");
    if (!out_ctx || validate_format(rate, channels, bits) < 0) {
        return -1;
    }
    *out_ctx = NULL;

    audio_context_t *ctx = audio_context_create("capture");
    if (!ctx) {
        return -1;
    }

    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    channel_config.dma_desc_num = 6;
    channel_config.dma_frame_num = 240;
    if (i2s_new_channel(&channel_config, NULL, &ctx->channel) != ESP_OK) {
        free(ctx);
        return -1;
    }

    i2s_std_config_t std_config =
        make_i2s_config(rate, MYBOT_AUDIO_I2S_MIC_BCLK, MYBOT_AUDIO_I2S_MIC_WS, I2S_GPIO_UNUSED,
                        MYBOT_AUDIO_I2S_MIC_DIN);
    if (i2s_channel_init_std_mode(ctx->channel, &std_config) != ESP_OK) {
        i2s_del_channel(ctx->channel);
        free(ctx);
        return -1;
    }

    *out_ctx = ctx;
    return 0;
}

static int capture_start(void *opaque) {
    audio_context_t *ctx = opaque;
    ESP_LOGI(TAG, "event=sdk_adapter adapter=audio direction=capture action=start");
    if (!ctx || aosl_atomic_read(&ctx->started) || i2s_channel_enable(ctx->channel) != ESP_OK) {
        return -1;
    }
    aosl_atomic_set(&ctx->started, true);
    return 0;
}

static int capture_read(void *opaque, void *buffer, int frames) {
    audio_context_t *ctx = opaque;
    if (!ctx || !aosl_atomic_read(&ctx->started) || !buffer || frames <= 0 ||
        frames > AUDIO_MAX_FRAMES) {
        return -1;
    }

    size_t bytes_read = 0;
    esp_err_t err =
        i2s_channel_read(ctx->channel, ctx->scratch, (size_t)frames * sizeof(ctx->scratch[0]),
                         &bytes_read, AUDIO_IO_TIMEOUT_MS);
    if (err == ESP_ERR_TIMEOUT) {
        return 0;
    }
    if (err != ESP_OK) {
        return -1;
    }

    int samples = (int)(bytes_read / sizeof(ctx->scratch[0]));
    int16_t *output = buffer;
    for (int i = 0; i < samples; ++i) {
        int32_t value = ctx->scratch[i] >> 12;
        if (value > INT16_MAX) {
            value = INT16_MAX;
        } else if (value < INT16_MIN) {
            value = INT16_MIN;
        }
        output[i] = (int16_t)value;
    }
    return samples;
}

static int audio_stop(void *opaque) {
    audio_context_t *ctx = opaque;
    if (!ctx) {
        return -1;
    }
    if (!aosl_atomic_read(&ctx->started)) {
        return 0;
    }
    ESP_LOGI(TAG, "event=sdk_adapter adapter=audio direction=%s action=stop", ctx->direction);
    esp_err_t err = i2s_channel_disable(ctx->channel);
    aosl_atomic_set(&ctx->started, false);
    return err == ESP_OK ? 0 : -1;
}

static void audio_destroy(void *opaque) {
    audio_context_t *ctx = opaque;
    if (!ctx) {
        return;
    }
    ESP_LOGI(TAG, "event=sdk_adapter adapter=audio direction=%s action=destroy", ctx->direction);
    if (aosl_atomic_read(&ctx->started)) {
        i2s_channel_disable(ctx->channel);
    }
    i2s_del_channel(ctx->channel);
    free(ctx);
}

static int playback_init(void **out_ctx, int rate, int channels, int bits) {
    ESP_LOGI(TAG, "event=sdk_adapter adapter=audio direction=playback action=initialize");
    if (!out_ctx || validate_format(rate, channels, bits) < 0) {
        return -1;
    }
    *out_ctx = NULL;
    apply_output_volume(MYBOT_AUDIO_VOLUME_MAX);

    audio_context_t *ctx = audio_context_create("playback");
    if (!ctx) {
        return -1;
    }

    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel_config.dma_desc_num = 6;
    channel_config.dma_frame_num = 240;
    channel_config.auto_clear_after_cb = true;
    if (i2s_new_channel(&channel_config, &ctx->channel, NULL) != ESP_OK) {
        free(ctx);
        return -1;
    }

    i2s_std_config_t std_config =
        make_i2s_config(rate, MYBOT_AUDIO_I2S_SPK_BCLK, MYBOT_AUDIO_I2S_SPK_WS,
                        MYBOT_AUDIO_I2S_SPK_DOUT, I2S_GPIO_UNUSED);
    if (i2s_channel_init_std_mode(ctx->channel, &std_config) != ESP_OK) {
        i2s_del_channel(ctx->channel);
        free(ctx);
        return -1;
    }

    *out_ctx = ctx;
    return 0;
}

static int playback_start(void *opaque) {
    audio_context_t *ctx = opaque;
    ESP_LOGI(TAG, "event=sdk_adapter adapter=audio direction=playback action=start");
    if (!ctx || aosl_atomic_read(&ctx->started) || i2s_channel_enable(ctx->channel) != ESP_OK) {
        return -1;
    }
    aosl_atomic_set(&ctx->started, true);
    return 0;
}

static int playback_write(void *opaque, const void *buffer, int frames) {
    audio_context_t *ctx = opaque;
    if (!ctx || !aosl_atomic_read(&ctx->started) || !buffer || frames <= 0 ||
        frames > AUDIO_MAX_FRAMES) {
        return -1;
    }

    const int16_t *input = buffer;
    int32_t gain_q16 = (int32_t)aosl_atomic_read(&s_output_gain_q16);
    for (int i = 0; i < frames; ++i) {
        ctx->scratch[i] = (int32_t)((int64_t)input[i] * gain_q16);
    }

    size_t bytes_written = 0;
    esp_err_t err =
        i2s_channel_write(ctx->channel, ctx->scratch, (size_t)frames * sizeof(ctx->scratch[0]),
                          &bytes_written, AUDIO_IO_TIMEOUT_MS);
    if (err == ESP_ERR_TIMEOUT) {
        return 0;
    }
    if (err != ESP_OK) {
        return -1;
    }
    return (int)(bytes_written / sizeof(ctx->scratch[0]));
}

static const mybot_audio_capture_ops_t s_capture_ops = {
    .init = capture_init,
    .start = capture_start,
    .read = capture_read,
    .stop = audio_stop,
    .destroy = audio_destroy,
};

static const mybot_audio_playback_ops_t s_playback_ops = {
    .init = playback_init,
    .start = playback_start,
    .write = playback_write,
    .stop = audio_stop,
    .destroy = audio_destroy,
};

static const mybot_audio_volume_ops_t s_volume_ops = {
    .init = volume_init,
    .set_volume = volume_set,
    .get_volume = volume_get,
    .destroy = volume_destroy,
};

const mybot_audio_capture_ops_t *mybot_esp32s3_audio_capture_ops(void) {
    return &s_capture_ops;
}

const mybot_audio_playback_ops_t *mybot_esp32s3_audio_playback_ops(void) {
    return &s_playback_ops;
}

const mybot_audio_volume_ops_t *mybot_esp32s3_audio_volume_ops(void) {
    return &s_volume_ops;
}
