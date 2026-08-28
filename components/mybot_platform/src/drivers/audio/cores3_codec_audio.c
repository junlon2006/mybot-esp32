/* SPDX-License-Identifier: Apache-2.0 */
#include "board_config.h"
#include "cores3_hardware.h"

#include <mybot/platform/mybot_audio.h>
#include <api/aosl_atomic.h>

#include "aw88298_dac.h"
#include "driver/i2s_tdm.h"
#include "es7210_adc.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define AUDIO_CONTEXT_MAGIC 0x43533341U
#define AUDIO_IO_TIMEOUT_MS 50
#define AUDIO_MAX_FRAMES 960
#define AUDIO_BUS_SLOTS 2
#define AUDIO_RX_DMA_CHANNELS 1
#define AUDIO_TX_DMA_CHANNELS 1
#define AUDIO_VOLUME_DEFAULT 70
#define AUDIO_VOLUME_NVS_KEY "output_volume"
#define AUDIO_VOLUME_NVS_NAMESPACE "audio"
#define TAG "cores3_audio"

typedef enum {
    AUDIO_DIRECTION_CAPTURE,
    AUDIO_DIRECTION_PLAYBACK,
} audio_direction_t;

typedef struct {
    uint32_t magic;
    audio_direction_t direction;
    aosl_atomic_t started;
    int16_t *scratch;
} audio_stream_context_t;

typedef struct {
    i2s_chan_handle_t tx_channel;
    i2s_chan_handle_t rx_channel;
    const audio_codec_ctrl_if_t *output_ctrl_if;
    const audio_codec_if_t *output_codec_if;
    const audio_codec_ctrl_if_t *input_ctrl_if;
    const audio_codec_if_t *input_codec_if;
    const audio_codec_gpio_if_t *gpio_if;
    esp_codec_dev_handle_t output_device;
    esp_codec_dev_handle_t input_device;
    unsigned references;
    bool ready;
    bool tx_enabled;
    bool rx_enabled;
    bool capture_started;
    bool playback_started;
} cores3_audio_shared_t;

typedef struct {
    nvs_handle_t handle;
    bool active;
    bool persisted_volume_known;
    bool persist_dirty;
    int persisted_volume;
} audio_volume_context_t;

static cores3_audio_shared_t s_audio;
static audio_volume_context_t s_volume_context;
static int s_output_volume = AUDIO_VOLUME_DEFAULT;
static StaticSemaphore_t s_audio_mutex_storage;
static SemaphoreHandle_t s_audio_mutex;
static portMUX_TYPE s_audio_mutex_init_lock = portMUX_INITIALIZER_UNLOCKED;

/* Codec control is provided by esp_codec_dev; PCM transfer uses bounded I2S calls below. */
static bool codec_data_is_open(const audio_codec_data_if_t *data_if) {
    return data_if != NULL;
}

static int codec_data_enable(const audio_codec_data_if_t *data_if, esp_codec_dev_type_t type,
                             bool enable) {
    (void)data_if;
    (void)type;
    (void)enable;
    return ESP_CODEC_DEV_OK;
}

static int codec_data_set_format(const audio_codec_data_if_t *data_if, esp_codec_dev_type_t type,
                                 esp_codec_dev_sample_info_t *format) {
    (void)data_if;
    (void)type;
    (void)format;
    return ESP_CODEC_DEV_OK;
}

static int codec_data_read(const audio_codec_data_if_t *data_if, uint8_t *data, int size) {
    (void)data_if;
    (void)data;
    (void)size;
    return ESP_CODEC_DEV_NOT_SUPPORT;
}

static int codec_data_write(const audio_codec_data_if_t *data_if, uint8_t *data, int size) {
    (void)data_if;
    (void)data;
    (void)size;
    return ESP_CODEC_DEV_NOT_SUPPORT;
}

static const audio_codec_data_if_t s_codec_data_if = {
    .is_open = codec_data_is_open,
    .enable = codec_data_enable,
    .set_fmt = codec_data_set_format,
    .read = codec_data_read,
    .write = codec_data_write,
};

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

static int validate_format(int rate, int channels, int bits) {
    return rate == MYBOT_AUDIO_SAMPLE_RATE && channels == 1 && bits == 16 ? 0 : -1;
}

static const char *direction_name(audio_direction_t direction) {
    return direction == AUDIO_DIRECTION_CAPTURE ? "capture" : "playback";
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

static void shared_release_locked(void) {
    (void)disable_channel(s_audio.rx_channel, &s_audio.rx_enabled, "capture");
    (void)disable_channel(s_audio.tx_channel, &s_audio.tx_enabled, "playback");

    if (s_audio.input_device) {
        if (esp_codec_dev_close(s_audio.input_device) != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "event=audio_device direction=capture action=close result=error");
        }
        esp_codec_dev_delete(s_audio.input_device);
    }
    if (s_audio.output_device) {
        if (esp_codec_dev_close(s_audio.output_device) != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "event=audio_device direction=playback action=close result=error");
        }
        esp_codec_dev_delete(s_audio.output_device);
    }
    if (s_audio.input_codec_if) {
        if (audio_codec_delete_codec_if(s_audio.input_codec_if) != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "event=audio_device component=es7210 action=destroy result=error");
        }
    }
    if (s_audio.input_ctrl_if) {
        if (audio_codec_delete_ctrl_if(s_audio.input_ctrl_if) != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "event=audio_device component=es7210_i2c action=destroy result=error");
        }
    }
    if (s_audio.output_codec_if) {
        if (audio_codec_delete_codec_if(s_audio.output_codec_if) != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "event=audio_device component=aw88298 action=destroy result=error");
        }
    }
    if (s_audio.output_ctrl_if) {
        if (audio_codec_delete_ctrl_if(s_audio.output_ctrl_if) != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "event=audio_device component=aw88298_i2c action=destroy result=error");
        }
    }
    if (s_audio.gpio_if) {
        if (audio_codec_delete_gpio_if(s_audio.gpio_if) != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "event=audio_device component=gpio action=destroy result=error");
        }
    }
    if (s_audio.rx_channel) {
        (void)disable_channel(s_audio.rx_channel, &s_audio.rx_enabled, "capture");
        esp_err_t err = i2s_del_channel(s_audio.rx_channel);
        if (err != ESP_OK) {
            ESP_LOGW(TAG,
                     "event=audio_device direction=capture action=destroy result=error "
                     "error=%s",
                     esp_err_to_name(err));
        }
    }
    if (s_audio.tx_channel) {
        (void)disable_channel(s_audio.tx_channel, &s_audio.tx_enabled, "playback");
        esp_err_t err = i2s_del_channel(s_audio.tx_channel);
        if (err != ESP_OK) {
            ESP_LOGW(TAG,
                     "event=audio_device direction=playback action=destroy result=error "
                     "error=%s",
                     esp_err_to_name(err));
        }
    }

    s_audio = (cores3_audio_shared_t){0};
}

static int initialize_i2s_locked(void) {
    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel_config.dma_desc_num = 6;
    channel_config.dma_frame_num = 240;
    channel_config.auto_clear_after_cb = true;
    esp_err_t err = i2s_new_channel(&channel_config, &s_audio.tx_channel, &s_audio.rx_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "event=audio_device action=initialize result=error reason=i2s_channel "
                 "error=%s",
                 esp_err_to_name(err));
        return -1;
    }

    /* IDF 5.5 shares clocks only for identical TX/RX modes. Two-slot Philips TDM is
     * wire-compatible with the AW88298 standard-I2S input while retaining ES7210 slot 0. */
    i2s_tdm_config_t duplex_config = {
        .clk_cfg =
            {
                .sample_rate_hz = MYBOT_AUDIO_SAMPLE_RATE,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .ext_clk_freq_hz = 0,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
                .bclk_div = 8,
            },
        .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO, I2S_TDM_SLOT0),
        .gpio_cfg =
            {
                .mclk = MYBOT_AUDIO_I2S_MCLK,
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
    duplex_config.slot_cfg.total_slot = AUDIO_BUS_SLOTS;
    duplex_config.slot_cfg.ws_width = I2S_DATA_BIT_WIDTH_16BIT;
    duplex_config.slot_cfg.left_align = true;

    err = i2s_channel_init_tdm_mode(s_audio.tx_channel, &duplex_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "event=audio_device direction=playback action=initialize result=error "
                 "reason=i2s_mode error=%s",
                 esp_err_to_name(err));
        return -1;
    }
    err = i2s_channel_init_tdm_mode(s_audio.rx_channel, &duplex_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "event=audio_device direction=capture action=initialize result=error "
                 "reason=i2s_mode error=%s",
                 esp_err_to_name(err));
        return -1;
    }
    return 0;
}

static int initialize_codecs_locked(void) {
    void *i2c_bus = mybot_cores3_i2c_bus_handle();
    if (!i2c_bus) {
        ESP_LOGE(TAG, "event=audio_device action=initialize result=error reason=i2c_bus");
        return -1;
    }

    s_audio.gpio_if = audio_codec_new_gpio();
    if (!s_audio.gpio_if) {
        goto no_memory;
    }

    audio_codec_i2c_cfg_t i2c_config = {
        .port = I2C_NUM_1,
        .addr = AW88298_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus,
    };
    s_audio.output_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_config);
    if (!s_audio.output_ctrl_if) {
        goto codec_error;
    }

    aw88298_codec_cfg_t output_codec_config = {
        .ctrl_if = s_audio.output_ctrl_if,
        .gpio_if = s_audio.gpio_if,
        .reset_pin = GPIO_NUM_NC,
        .hw_gain =
            {
                .pa_voltage = 5.0f,
                .codec_dac_voltage = 3.3f,
                .pa_gain = 1.0f,
            },
    };
    s_audio.output_codec_if = aw88298_codec_new(&output_codec_config);
    if (!s_audio.output_codec_if) {
        goto codec_error;
    }

    esp_codec_dev_cfg_t device_config = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = s_audio.output_codec_if,
        .data_if = &s_codec_data_if,
    };
    s_audio.output_device = esp_codec_dev_new(&device_config);
    if (!s_audio.output_device) {
        goto no_memory;
    }

    i2c_config.addr = ES7210_CODEC_DEFAULT_ADDR;
    s_audio.input_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_config);
    if (!s_audio.input_ctrl_if) {
        goto codec_error;
    }

    es7210_codec_cfg_t input_codec_config = {
        .ctrl_if = s_audio.input_ctrl_if,
        .master_mode = false,
        .mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3,
        .mclk_src = ES7210_MCLK_FROM_PAD,
        .mclk_div = 256,
    };
    s_audio.input_codec_if = es7210_codec_new(&input_codec_config);
    if (!s_audio.input_codec_if) {
        goto codec_error;
    }

    device_config.dev_type = ESP_CODEC_DEV_TYPE_IN;
    device_config.codec_if = s_audio.input_codec_if;
    s_audio.input_device = esp_codec_dev_new(&device_config);
    if (!s_audio.input_device) {
        goto no_memory;
    }
    return 0;

no_memory:
    ESP_LOGE(TAG, "event=audio_device action=initialize result=error reason=no_memory");
    return -1;
codec_error:
    ESP_LOGE(TAG, "event=audio_device action=initialize result=error reason=codec_control");
    return -1;
}

static int shared_acquire_locked(void) {
    if (s_audio.ready) {
        ++s_audio.references;
        return 0;
    }
    if (initialize_i2s_locked() < 0) {
        shared_release_locked();
        return -1;
    }
    if (mybot_cores3_reset_audio_codec() < 0) {
        ESP_LOGE(TAG, "event=audio_device component=aw88298 action=reset result=error");
        shared_release_locked();
        return -1;
    }
    if (initialize_codecs_locked() < 0) {
        shared_release_locked();
        return -1;
    }
    s_audio.ready = true;
    s_audio.references = 1;
    ESP_LOGI(TAG,
             "event=audio_device action=initialize rate=%d bus=tdm2_slot0 duplex=1 "
             "result=ok",
             MYBOT_AUDIO_SAMPLE_RATE);
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
    size_t samples = direction == AUDIO_DIRECTION_CAPTURE
                         ? AUDIO_MAX_FRAMES * AUDIO_RX_DMA_CHANNELS
                         : AUDIO_MAX_FRAMES * AUDIO_TX_DMA_CHANNELS;
    context->scratch = calloc(samples, sizeof(*context->scratch));
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
    int result = shared_acquire_locked();
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
            disable_channel(s_audio.tx_channel, &s_audio.tx_enabled, "playback");
        }
        goto done;
    }
    s_audio.rx_enabled = true;

    esp_codec_dev_sample_info_t format = {
        .bits_per_sample = 16,
        .channel = 2,
        .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
        .sample_rate = MYBOT_AUDIO_SAMPLE_RATE,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };
    if (esp_codec_dev_open(s_audio.input_device, &format) != ESP_CODEC_DEV_OK ||
        esp_codec_dev_set_in_channel_gain(s_audio.input_device, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
                                          30.0f) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "event=audio_device direction=capture action=start result=error "
                      "reason=codec");
        esp_codec_dev_close(s_audio.input_device);
        disable_channel(s_audio.rx_channel, &s_audio.rx_enabled, "capture");
        if (!s_audio.playback_started) {
            disable_channel(s_audio.tx_channel, &s_audio.tx_enabled, "playback");
        }
        goto done;
    }

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
    esp_codec_dev_sample_info_t format = {
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
        .sample_rate = MYBOT_AUDIO_SAMPLE_RATE,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };
    if (esp_codec_dev_open(s_audio.output_device, &format) != ESP_CODEC_DEV_OK ||
        esp_codec_dev_set_out_vol(s_audio.output_device, s_output_volume) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "event=audio_device direction=playback action=start result=error "
                      "reason=codec");
        esp_codec_dev_close(s_audio.output_device);
        if (!s_audio.capture_started) {
            disable_channel(s_audio.tx_channel, &s_audio.tx_enabled, "playback");
        }
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
                         (size_t)frames * AUDIO_RX_DMA_CHANNELS * sizeof(context->scratch[0]),
                         &bytes_read, AUDIO_IO_TIMEOUT_MS);
    if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
        return aosl_atomic_read(&context->started) ? -1 : 0;
    }

    int captured_frames = (int)(bytes_read / (AUDIO_RX_DMA_CHANNELS * sizeof(context->scratch[0])));
    int16_t *output = buffer;
    for (int frame = 0; frame < captured_frames; ++frame) {
        output[frame] = context->scratch[frame * AUDIO_RX_DMA_CHANNELS];
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
    for (int frame = 0; frame < frames; ++frame) {
        context->scratch[frame * AUDIO_TX_DMA_CHANNELS] = input[frame];
    }

    size_t bytes_written = 0;
    esp_err_t err =
        i2s_channel_write(s_audio.tx_channel, context->scratch,
                          (size_t)frames * AUDIO_TX_DMA_CHANNELS * sizeof(context->scratch[0]),
                          &bytes_written, AUDIO_IO_TIMEOUT_MS);
    if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
        return aosl_atomic_read(&context->started) ? -1 : 0;
    }
    return (int)(bytes_written / (AUDIO_TX_DMA_CHANNELS * sizeof(context->scratch[0])));
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
        if (!s_audio.playback_started) {
            if (disable_channel(s_audio.tx_channel, &s_audio.tx_enabled, "playback") < 0) {
                result = -1;
            }
        }
        if (esp_codec_dev_close(s_audio.input_device) != ESP_CODEC_DEV_OK) {
            result = -1;
        }
    } else {
        s_audio.playback_started = false;
        if (!s_audio.capture_started) {
            if (disable_channel(s_audio.tx_channel, &s_audio.tx_enabled, "playback") < 0) {
                result = -1;
            }
        }
        if (esp_codec_dev_close(s_audio.output_device) != ESP_CODEC_DEV_OK) {
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
        if (s_audio.references > 0) {
            --s_audio.references;
        }
        if (s_audio.references == 0) {
            shared_release_locked();
            ESP_LOGI(TAG, "event=audio_device action=destroy result=ok");
        }
        audio_unlock();
    }
    context->magic = 0;
    free(context->scratch);
    free(context);
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

static void erase_invalid_volume(nvs_handle_t handle) {
    esp_err_t err = nvs_erase_key(handle, AUDIO_VOLUME_NVS_KEY);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "event=volume action=erase_invalid result=error reason=erase error=%s",
                 esp_err_to_name(err));
    } else if (err == ESP_OK && nvs_commit(handle) != ESP_OK) {
        ESP_LOGW(TAG, "event=volume action=erase_invalid result=error reason=commit");
    }
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
    if (audio_lock()) {
        s_output_volume = volume;
        audio_unlock();
    }
    *out_context = &s_volume_context;
    ESP_LOGI(TAG, "event=volume action=restore value=%d source=%s result=ok", volume, source);
    return 0;
}

static int volume_set(void *opaque, int volume) {
    audio_volume_context_t *context = opaque;
    if (context != &s_volume_context || !context->active || volume < MYBOT_AUDIO_VOLUME_MIN ||
        volume > MYBOT_AUDIO_VOLUME_MAX || !audio_lock()) {
        return -1;
    }

    int previous_volume = s_output_volume;
    if (s_audio.playback_started &&
        esp_codec_dev_set_out_vol(s_audio.output_device, volume) != ESP_CODEC_DEV_OK) {
        audio_unlock();
        ESP_LOGE(TAG, "event=volume action=set value=%d result=error reason=codec", volume);
        return -1;
    }
    s_output_volume = volume;
    audio_unlock();

    if (previous_volume == volume && !context->persist_dirty && context->persisted_volume_known &&
        context->persisted_volume == volume) {
        return 0;
    }
    if (persist_volume(context, volume) < 0) {
        context->persist_dirty = true;
        ESP_LOGW(TAG, "event=volume action=set value=%d applied=1 persisted=0 result=error "
                      "scope=persistence");
        return 0;
    }

    ESP_LOGI(TAG, "event=volume action=set value=%d persisted=1 result=ok", volume);
    return 0;
}

static int volume_get(void *opaque, int *volume) {
    audio_volume_context_t *context = opaque;
    if (context != &s_volume_context || !context->active || !volume || !audio_lock()) {
        return -1;
    }
    *volume = s_output_volume;
    audio_unlock();
    return 0;
}

static void volume_destroy(void *opaque) {
    audio_volume_context_t *context = opaque;
    if (context != &s_volume_context || !context->active) {
        return;
    }

    int volume = s_output_volume;
    bool persisted = context->persisted_volume_known && context->persisted_volume == volume;
    if (context->persist_dirty) {
        persisted = persist_volume(context, volume) == 0;
    }
    if (context->persist_dirty) {
        ESP_LOGW(TAG, "event=volume action=detach value=%d persisted=0 result=error "
                      "scope=persistence");
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

const mybot_audio_capture_ops_t *mybot_cores3_audio_capture_ops(void) {
    return &s_capture_ops;
}

const mybot_audio_playback_ops_t *mybot_cores3_audio_playback_ops(void) {
    return &s_playback_ops;
}

const mybot_audio_volume_ops_t *mybot_cores3_audio_volume_ops(void) {
    return &s_volume_ops;
}
