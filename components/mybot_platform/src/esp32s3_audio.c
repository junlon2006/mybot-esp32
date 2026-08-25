/* SPDX-License-Identifier: Apache-2.0 */
#include "board_config.h"

#include <mybot/platform/mybot_audio.h>
#include <api/aosl_atomic.h>

#include "driver/i2s_std.h"
#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define AUDIO_MAX_FRAMES 960
#define AUDIO_IO_TIMEOUT_MS 50

typedef struct {
    i2s_chan_handle_t channel;
    aosl_atomic_t started;
    int32_t scratch[AUDIO_MAX_FRAMES];
} audio_context_t;

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

static audio_context_t *audio_context_create(void) {
    return calloc(1, sizeof(audio_context_t));
}

static int capture_init(void **out_ctx, int rate, int channels, int bits) {
    if (!out_ctx || validate_format(rate, channels, bits) < 0) {
        return -1;
    }
    *out_ctx = NULL;

    audio_context_t *ctx = audio_context_create();
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
    esp_err_t err = i2s_channel_disable(ctx->channel);
    aosl_atomic_set(&ctx->started, false);
    return err == ESP_OK ? 0 : -1;
}

static void audio_destroy(void *opaque) {
    audio_context_t *ctx = opaque;
    if (!ctx) {
        return;
    }
    if (aosl_atomic_read(&ctx->started)) {
        i2s_channel_disable(ctx->channel);
    }
    i2s_del_channel(ctx->channel);
    free(ctx);
}

static int playback_init(void **out_ctx, int rate, int channels, int bits) {
    if (!out_ctx || validate_format(rate, channels, bits) < 0) {
        return -1;
    }
    *out_ctx = NULL;

    audio_context_t *ctx = audio_context_create();
    if (!ctx) {
        return -1;
    }

    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel_config.dma_desc_num = 6;
    channel_config.dma_frame_num = 240;
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
    for (int i = 0; i < frames; ++i) {
        ctx->scratch[i] = (int32_t)input[i] * 65536;
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

const mybot_audio_capture_ops_t *mybot_esp32s3_audio_capture_ops(void) {
    return &s_capture_ops;
}

const mybot_audio_playback_ops_t *mybot_esp32s3_audio_playback_ops(void) {
    return &s_playback_ops;
}
