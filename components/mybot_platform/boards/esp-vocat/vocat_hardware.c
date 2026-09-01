/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#include "vocat_hardware.h"

#include "board_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>

#define TAG "vocat_hw"
#define I2C_PROBE_TIMEOUT_MS 100

typedef enum {
    VOCAT_REVISION_UNKNOWN,
    VOCAT_REVISION_V1_0,
    VOCAT_REVISION_V1_2,
} vocat_revision_t;

typedef struct {
    i2c_master_bus_handle_t bus;
    vocat_revision_t revision;
    gpio_num_t audio_din;
    gpio_num_t audio_pa;
    gpio_num_t lcd_reset;
    bool lcd_reset_active_high;
    bool pa_ready;
    bool initialized;
} vocat_hardware_t;

static vocat_hardware_t s_hardware = {
    .audio_din = GPIO_NUM_NC,
    .audio_pa = GPIO_NUM_NC,
    .lcd_reset = GPIO_NUM_NC,
};

static const char *revision_name(vocat_revision_t revision) {
    switch (revision) {
    case VOCAT_REVISION_V1_0:
        return "v1.0";
    case VOCAT_REVISION_V1_2:
        return "v1.2";
    default:
        return "unknown";
    }
}

static esp_err_t configure_control_pins(void) {
    const gpio_config_t config = {
        .pin_bit_mask = (1ULL << MYBOT_VOCAT_CODEC_SELECT) | (1ULL << MYBOT_VOCAT_POWER_CONTROL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t result = gpio_config(&config);
    if (result == ESP_OK) {
        result = gpio_set_level(MYBOT_VOCAT_POWER_CONTROL, 0);
    }
    if (result == ESP_OK) {
        result = gpio_set_level(MYBOT_VOCAT_CODEC_SELECT, 0);
    }
    return result;
}

static esp_err_t probe_revision_candidate(vocat_revision_t revision, int select_level) {
    esp_err_t result = gpio_set_level(MYBOT_VOCAT_CODEC_SELECT, select_level);
    if (result != ESP_OK) {
        ESP_LOGE(TAG,
                 "event=board_revision action=select candidate=%s level=%d result=error code=%s",
                 revision_name(revision), select_level, esp_err_to_name(result));
        return result;
    }

    vTaskDelay(pdMS_TO_TICKS(MYBOT_VOCAT_REVISION_PROBE_DELAY_MS));
    result =
        i2c_master_probe(s_hardware.bus, MYBOT_VOCAT_ES8311_PROBE_ADDRESS, I2C_PROBE_TIMEOUT_MS);
    if (result == ESP_OK) {
        ESP_LOGI(TAG,
                 "event=board_revision action=probe candidate=%s select_level=%d "
                 "codec=es8311 address=0x%02x result=present",
                 revision_name(revision), select_level, MYBOT_VOCAT_ES8311_PROBE_ADDRESS);
    } else {
        ESP_LOGW(TAG,
                 "event=board_revision action=probe candidate=%s select_level=%d "
                 "codec=es8311 address=0x%02x result=absent code=%s",
                 revision_name(revision), select_level, MYBOT_VOCAT_ES8311_PROBE_ADDRESS,
                 esp_err_to_name(result));
    }
    return result;
}

static void freeze_revision(vocat_revision_t revision) {
    s_hardware.revision = revision;
    if (revision == VOCAT_REVISION_V1_0) {
        s_hardware.audio_din = MYBOT_VOCAT_V10_AUDIO_DIN;
        s_hardware.audio_pa = MYBOT_VOCAT_V10_AUDIO_PA;
        s_hardware.lcd_reset = MYBOT_VOCAT_V10_LCD_RESET;
        s_hardware.lcd_reset_active_high = MYBOT_VOCAT_V10_LCD_RESET_ACTIVE_HIGH;
    } else {
        s_hardware.audio_din = MYBOT_VOCAT_V12_AUDIO_DIN;
        s_hardware.audio_pa = MYBOT_VOCAT_V12_AUDIO_PA;
        s_hardware.lcd_reset = MYBOT_VOCAT_V12_LCD_RESET;
        s_hardware.lcd_reset_active_high = MYBOT_VOCAT_V12_LCD_RESET_ACTIVE_HIGH;
    }
}

static esp_err_t detect_revision(void) {
    esp_err_t first = probe_revision_candidate(VOCAT_REVISION_V1_0, 0);
    if (first == ESP_OK) {
        freeze_revision(VOCAT_REVISION_V1_0);
        return ESP_OK;
    }

    esp_err_t second = probe_revision_candidate(VOCAT_REVISION_V1_2, 1);
    if (second == ESP_OK) {
        freeze_revision(VOCAT_REVISION_V1_2);
        return ESP_OK;
    }

    ESP_LOGE(TAG,
             "event=board_revision action=detect result=error reason=codec_not_found "
             "v1_0_code=%s v1_2_code=%s",
             esp_err_to_name(first), esp_err_to_name(second));
    return second;
}

static esp_err_t initialize_pa(void) {
    if (s_hardware.audio_pa == GPIO_NUM_NC) {
        return ESP_ERR_INVALID_STATE;
    }
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << s_hardware.audio_pa,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t result = gpio_config(&config);
    if (result == ESP_OK) {
        result = gpio_set_level(s_hardware.audio_pa, 0);
    }
    if (result == ESP_OK) {
        s_hardware.pa_ready = true;
    }
    return result;
}

int mybot_vocat_hardware_deinit(void) {
    int result = 0;

    if (s_hardware.pa_ready && gpio_set_level(s_hardware.audio_pa, 0) != ESP_OK) {
        ESP_LOGW(TAG, "event=hardware action=deinitialize component=speaker_pa result=error");
        result = -1;
    }
    if (s_hardware.bus) {
        esp_err_t err = i2c_del_master_bus(s_hardware.bus);
        if (err == ESP_OK) {
            s_hardware.bus = NULL;
        } else {
            ESP_LOGW(TAG,
                     "event=hardware action=deinitialize component=i2c_bus result=error code=%s",
                     esp_err_to_name(err));
            result = -1;
        }
    }

    s_hardware.initialized = false;
    if (!s_hardware.bus) {
        s_hardware = (vocat_hardware_t){
            .audio_din = GPIO_NUM_NC,
            .audio_pa = GPIO_NUM_NC,
            .lcd_reset = GPIO_NUM_NC,
        };
    }
    ESP_LOGI(TAG, "event=hardware action=deinitialize result=%s", result == 0 ? "ok" : "error");
    return result;
}

int mybot_vocat_hardware_init(void) {
    if (s_hardware.initialized) {
        return 0;
    }
    if (s_hardware.bus && mybot_vocat_hardware_deinit() < 0) {
        ESP_LOGE(TAG, "event=hardware action=initialize result=error reason=cleanup_pending");
        return -1;
    }

    esp_err_t result = configure_control_pins();
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = MYBOT_VOCAT_I2C_PORT,
        .sda_io_num = MYBOT_VOCAT_I2C_SDA,
        .scl_io_num = MYBOT_VOCAT_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (result == ESP_OK) {
        result = i2c_new_master_bus(&bus_config, &s_hardware.bus);
    }
    if (result == ESP_OK) {
        result = detect_revision();
    }
    if (result == ESP_OK) {
        result = initialize_pa();
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "event=hardware action=initialize result=error code=%s",
                 esp_err_to_name(result));
        if (mybot_vocat_hardware_deinit() < 0) {
            ESP_LOGE(TAG, "event=hardware action=initialize cleanup=error");
        }
        return -1;
    }

    s_hardware.initialized = true;
    ESP_LOGI(TAG,
             "event=hardware action=initialize board_revision=%s i2c_port=%d "
             "audio_din=%d audio_pa=%d lcd_reset=%d lcd_reset_active_high=%d result=ok",
             revision_name(s_hardware.revision), MYBOT_VOCAT_I2C_PORT, s_hardware.audio_din,
             s_hardware.audio_pa, s_hardware.lcd_reset, s_hardware.lcd_reset_active_high ? 1 : 0);
    return 0;
}

i2c_master_bus_handle_t mybot_vocat_i2c_bus_handle(void) {
    return s_hardware.initialized ? s_hardware.bus : NULL;
}

gpio_num_t mybot_vocat_audio_din_gpio(void) {
    return s_hardware.initialized ? s_hardware.audio_din : GPIO_NUM_NC;
}

gpio_num_t mybot_vocat_audio_pa_gpio(void) {
    return s_hardware.initialized ? s_hardware.audio_pa : GPIO_NUM_NC;
}

gpio_num_t mybot_vocat_lcd_reset_gpio(void) {
    return s_hardware.initialized ? s_hardware.lcd_reset : GPIO_NUM_NC;
}

bool mybot_vocat_lcd_reset_active_high(void) {
    return s_hardware.initialized && s_hardware.lcd_reset_active_high;
}
