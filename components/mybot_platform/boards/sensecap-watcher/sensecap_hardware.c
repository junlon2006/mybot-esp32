/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#include "sensecap_hardware.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_io_expander.h"
#include "esp_io_expander_tca95xx_16bit.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stddef.h>
#include <stdint.h>

#define TAG "sensecap_hardware"
#define BACKLIGHT_LEDC_MODE LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_TIMER LEDC_TIMER_0
#define BACKLIGHT_LEDC_CHANNEL LEDC_CHANNEL_0
#define BACKLIGHT_LEDC_RESOLUTION LEDC_TIMER_13_BIT
#define BACKLIGHT_LEDC_FREQUENCY_HZ 5000
#define BACKLIGHT_MAX_DUTY ((1U << 13) - 1U)

_Static_assert((MYBOT_SENSECAP_EXPANDER_INPUT_MASK & MYBOT_SENSECAP_EXPANDER_OUTPUT_MASK) == 0,
               "SenseCAP expander input and output masks must not overlap");
_Static_assert((MYBOT_SENSECAP_EXPANDER_INPUT_MASK | MYBOT_SENSECAP_EXPANDER_OUTPUT_MASK) ==
                   UINT16_MAX,
               "SenseCAP expander masks must cover every TCA9555 pin");

typedef struct {
    i2c_master_bus_handle_t bus;
    esp_io_expander_handle_t expander;
    uint16_t output_state;
    bool backlight_ready;
    bool initialized;
} sensecap_hardware_t;

static sensecap_hardware_t s_hardware;
static StaticSemaphore_t s_output_mutex_storage;
static SemaphoreHandle_t s_output_mutex;

static esp_err_t prepare_unpowered_display_pins(void) {
    const gpio_config_t config = {
        .pin_bit_mask = (1ULL << MYBOT_SENSECAP_TOUCH_SDA) | (1ULL << MYBOT_SENSECAP_TOUCH_SCL) |
                        (1ULL << MYBOT_SENSECAP_LCD_PCLK) | (1ULL << MYBOT_SENSECAP_LCD_DATA0) |
                        (1ULL << MYBOT_SENSECAP_LCD_DATA1) | (1ULL << MYBOT_SENSECAP_LCD_DATA2) |
                        (1ULL << MYBOT_SENSECAP_LCD_DATA3) | (1ULL << MYBOT_SENSECAP_LCD_CS) |
                        (1ULL << MYBOT_DISPLAY_BACKLIGHT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t result = gpio_config(&config);
    if (result != ESP_OK) {
        return result;
    }

    const gpio_num_t pins[] = {
        MYBOT_SENSECAP_TOUCH_SDA, MYBOT_SENSECAP_TOUCH_SCL, MYBOT_SENSECAP_LCD_PCLK,
        MYBOT_SENSECAP_LCD_DATA0, MYBOT_SENSECAP_LCD_DATA1, MYBOT_SENSECAP_LCD_DATA2,
        MYBOT_SENSECAP_LCD_DATA3, MYBOT_SENSECAP_LCD_CS,    MYBOT_DISPLAY_BACKLIGHT,
    };
    for (size_t index = 0; index < sizeof(pins) / sizeof(pins[0]); ++index) {
        result = gpio_set_level(pins[index], 0);
        if (result != ESP_OK) {
            return result;
        }
    }
    return ESP_OK;
}

static esp_err_t initialize_backlight(void) {
    const ledc_timer_config_t timer_config = {
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .duty_resolution = BACKLIGHT_LEDC_RESOLUTION,
        .timer_num = BACKLIGHT_LEDC_TIMER,
        .freq_hz = BACKLIGHT_LEDC_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t result = ledc_timer_config(&timer_config);
    if (result != ESP_OK) {
        return result;
    }

    const ledc_channel_config_t channel_config = {
        .gpio_num = MYBOT_DISPLAY_BACKLIGHT,
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .channel = BACKLIGHT_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BACKLIGHT_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    result = ledc_channel_config(&channel_config);
    if (result == ESP_OK) {
        s_hardware.backlight_ready = true;
    }
    return result;
}

static esp_err_t apply_output_state_locked(uint16_t requested) {
    uint16_t changed = s_hardware.output_state ^ requested;
    uint16_t set_low = changed & (uint16_t)~requested;
    uint16_t set_high = changed & requested;
    esp_err_t result = ESP_OK;

    if (set_low != 0) {
        result = esp_io_expander_set_level(s_hardware.expander, set_low, 0);
        if (result == ESP_OK) {
            s_hardware.output_state &= (uint16_t)~set_low;
        }
    }
    if (result == ESP_OK && set_high != 0) {
        result = esp_io_expander_set_level(s_hardware.expander, set_high, 1);
        if (result == ESP_OK) {
            s_hardware.output_state |= set_high;
        }
    }
    return result;
}

static esp_err_t initialize_expander(void) {
    if (!s_hardware.expander->write_output_reg) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    /* The TCA driver resets its output latch high. Clear it while every pin is still an input so
     * unused power rails cannot pulse high when their directions change. */
    esp_err_t result = s_hardware.expander->write_output_reg(s_hardware.expander, 0);
    if (result == ESP_OK) {
        result = esp_io_expander_set_dir(s_hardware.expander, MYBOT_SENSECAP_EXPANDER_INPUT_MASK,
                                         IO_EXPANDER_INPUT);
    }
    if (result == ESP_OK) {
        result = esp_io_expander_set_dir(s_hardware.expander, MYBOT_SENSECAP_EXPANDER_OUTPUT_MASK,
                                         IO_EXPANDER_OUTPUT);
    }
    if (result != ESP_OK) {
        return result;
    }

    s_hardware.output_state = 0;
    result = apply_output_state_locked(MYBOT_SENSECAP_POWER_SYSTEM_MASK);
    if (result != ESP_OK) {
        return result;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    result =
        apply_output_state_locked(MYBOT_SENSECAP_POWER_SYSTEM_MASK | MYBOT_SENSECAP_POWER_LCD_MASK |
                                  MYBOT_SENSECAP_POWER_CODEC_PA_MASK);
    if (result == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return result;
}

static int release_failed_initialization(void) {
    int result = 0;
    if (s_hardware.expander) {
        if (s_hardware.expander->write_output_reg) {
            esp_err_t err = s_hardware.expander->write_output_reg(s_hardware.expander, 0);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "event=hardware action=cleanup component=power result=error code=%s",
                         esp_err_to_name(err));
                result = -1;
            }
        }
    }
    if (s_hardware.backlight_ready) {
        esp_err_t err = ledc_stop(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, 0);
        if (err == ESP_OK) {
            s_hardware.backlight_ready = false;
        } else {
            ESP_LOGW(TAG, "event=hardware action=cleanup component=backlight result=error code=%s",
                     esp_err_to_name(err));
            result = -1;
        }
    }
    if (s_hardware.expander) {
        esp_err_t err = esp_io_expander_del(s_hardware.expander);
        if (err == ESP_OK) {
            s_hardware.expander = NULL;
        } else {
            ESP_LOGW(TAG, "event=hardware action=cleanup component=expander result=error code=%s",
                     esp_err_to_name(err));
            result = -1;
        }
    }
    if (!s_hardware.expander && s_hardware.bus) {
        esp_err_t err = i2c_del_master_bus(s_hardware.bus);
        if (err == ESP_OK) {
            s_hardware.bus = NULL;
        } else {
            ESP_LOGW(TAG, "event=hardware action=cleanup component=i2c_bus result=error code=%s",
                     esp_err_to_name(err));
            result = -1;
        }
    }
    if (!s_hardware.expander && !s_hardware.bus && !s_hardware.backlight_ready) {
        s_hardware = (sensecap_hardware_t){0};
    } else {
        s_hardware.initialized = false;
        s_hardware.output_state = 0;
    }
    return result;
}

int mybot_sensecap_hardware_init(void) {
    if (s_hardware.initialized) {
        return 0;
    }
    if ((s_hardware.expander || s_hardware.bus || s_hardware.backlight_ready) &&
        release_failed_initialization() < 0) {
        ESP_LOGE(TAG, "event=hardware action=initialize result=error reason=cleanup_pending");
        return -1;
    }
    if (!s_output_mutex) {
        s_output_mutex = xSemaphoreCreateMutexStatic(&s_output_mutex_storage);
    }
    if (!s_output_mutex || prepare_unpowered_display_pins() != ESP_OK) {
        ESP_LOGE(TAG, "event=hardware action=prepare_pins result=error");
        return -1;
    }

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = MYBOT_SENSECAP_I2C_PORT,
        .sda_io_num = MYBOT_SENSECAP_I2C_SDA,
        .scl_io_num = MYBOT_SENSECAP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t result = i2c_new_master_bus(&bus_config, &s_hardware.bus);
    if (result == ESP_OK) {
        result = esp_io_expander_new_i2c_tca95xx_16bit(
            s_hardware.bus, MYBOT_SENSECAP_TCA9555_ADDRESS, &s_hardware.expander);
    }
    if (result == ESP_OK) {
        result = initialize_backlight();
    }
    if (result == ESP_OK) {
        result = initialize_expander();
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "event=hardware action=initialize result=error code=%s",
                 esp_err_to_name(result));
        if (release_failed_initialization() < 0) {
            ESP_LOGE(TAG, "event=hardware action=initialize cleanup=error");
        }
        return -1;
    }

    s_hardware.initialized = true;
    ESP_LOGI(TAG,
             "event=hardware action=initialize result=ok i2c_port=%d expander=0x%02x "
             "power=system,lcd,codec_pa",
             MYBOT_SENSECAP_I2C_PORT, MYBOT_SENSECAP_TCA9555_ADDRESS);
    return 0;
}

i2c_master_bus_handle_t mybot_sensecap_i2c_bus_handle(void) {
    return s_hardware.initialized ? s_hardware.bus : NULL;
}

static int set_expander_power(uint16_t mask, bool enabled, const char *component) {
    if (!s_hardware.initialized || !s_hardware.expander || !s_output_mutex ||
        xSemaphoreTake(s_output_mutex, portMAX_DELAY) != pdTRUE) {
        return -1;
    }

    uint16_t requested = enabled ? (uint16_t)(s_hardware.output_state | mask)
                                 : (uint16_t)(s_hardware.output_state & (uint16_t)~mask);
    esp_err_t result = ESP_OK;
    if (requested != s_hardware.output_state) {
        result = apply_output_state_locked(requested);
    }
    xSemaphoreGive(s_output_mutex);

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "event=power component=%s enabled=%d result=error code=%s", component,
                 enabled ? 1 : 0, esp_err_to_name(result));
        return -1;
    }
    if (enabled) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_LOGI(TAG, "event=power component=%s enabled=%d result=ok", component, enabled ? 1 : 0);
    return 0;
}

int mybot_sensecap_set_codec_power(bool enabled) {
    return set_expander_power(MYBOT_SENSECAP_POWER_CODEC_PA_MASK, enabled, "codec_pa");
}

int mybot_sensecap_set_lcd_power(bool enabled) {
    if (!enabled) {
        (void)mybot_sensecap_set_display_backlight(0);
    }
    return set_expander_power(MYBOT_SENSECAP_POWER_LCD_MASK, enabled, "lcd");
}

int mybot_sensecap_set_display_backlight(unsigned int percent) {
    if (!s_hardware.initialized || !s_hardware.backlight_ready || percent > 100) {
        return -1;
    }
    const uint32_t duty = (percent * BACKLIGHT_MAX_DUTY + 50U) / 100U;
    esp_err_t result =
        ledc_set_duty_and_update(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty, 0);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "event=backlight percent=%u result=error code=%s", percent,
                 esp_err_to_name(result));
        return -1;
    }
    ESP_LOGI(TAG, "event=backlight percent=%u result=ok", percent);
    return 0;
}

esp_err_t mybot_sensecap_read_knob_button(bool *released) {
    if (!released || !s_hardware.initialized || !s_hardware.expander) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t inputs = 0;
    esp_err_t result =
        esp_io_expander_get_level(s_hardware.expander, MYBOT_SENSECAP_KNOB_BUTTON_MASK, &inputs);
    if (result == ESP_OK) {
        *released = (inputs & MYBOT_SENSECAP_KNOB_BUTTON_MASK) != 0;
    }
    return result;
}
