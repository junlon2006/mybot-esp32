/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#include "sticks3_hardware.h"

#include "M5PM1.h"
#include "board_config.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <new>

#define TAG "sticks3_hardware"
#define BACKLIGHT_LEDC_MODE LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_TIMER LEDC_TIMER_0
#define BACKLIGHT_LEDC_CHANNEL LEDC_CHANNEL_0
#define BACKLIGHT_LEDC_RESOLUTION LEDC_TIMER_13_BIT
#define BACKLIGHT_LEDC_FREQUENCY_HZ 5000
#define BACKLIGHT_MAX_DUTY ((1U << 13) - 1U)

typedef struct {
    i2c_master_bus_handle_t bus;
    M5PM1 *pmic;
    bool backlight_ready;
    bool speaker_powered;
    bool initialized;
} sticks3_hardware_t;

static sticks3_hardware_t s_hardware;
static StaticSemaphore_t s_hardware_mutex_storage;
static SemaphoreHandle_t s_hardware_mutex;

static esp_err_t initialize_backlight(void) {
    const ledc_timer_config_t timer_config = {
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .duty_resolution = BACKLIGHT_LEDC_RESOLUTION,
        .timer_num = BACKLIGHT_LEDC_TIMER,
        .freq_hz = BACKLIGHT_LEDC_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
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
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags =
            {
                .output_invert = 0,
            },
    };
    result = ledc_channel_config(&channel_config);
    if (result == ESP_OK) {
        s_hardware.backlight_ready = true;
    }
    return result;
}

static m5pm1_err_t configure_power_output(m5pm1_gpio_num_t pin, bool enabled) {
    return s_hardware.pmic->gpioSet(pin, M5PM1_GPIO_MODE_OUTPUT,
                                    enabled ? M5PM1_GPIO_STATE_HIGH : M5PM1_GPIO_STATE_LOW,
                                    M5PM1_GPIO_PULL_NONE, M5PM1_GPIO_DRIVE_PUSHPULL);
}

static m5pm1_err_t initialize_pmic(void) {
    s_hardware.pmic = new (std::nothrow) M5PM1();
    if (!s_hardware.pmic) {
        return M5PM1_ERR_INTERNAL;
    }

    m5pm1_err_t result = s_hardware.pmic->begin(s_hardware.bus, MYBOT_STICKS3_M5PM1_ADDRESS,
                                                MYBOT_STICKS3_I2C_SPEED_HZ);
    if (result == M5PM1_OK) {
        result = s_hardware.pmic->setChargeEnable(true);
    }
    if (result == M5PM1_OK) {
        result = s_hardware.pmic->setBoostEnable(false);
    }
    if (result == M5PM1_OK) {
        result = s_hardware.pmic->setDoubleOffDisable(true);
    }
    if (result == M5PM1_OK) {
        result = configure_power_output(M5PM1_GPIO_NUM_3, false);
    }
    if (result == M5PM1_OK) {
        result = configure_power_output(M5PM1_GPIO_NUM_2, true);
    }
    if (result == M5PM1_OK) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return result;
}

static int release_hardware(void) {
    int result = 0;
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
    if (s_hardware.pmic) {
        m5pm1_err_t speaker_result =
            s_hardware.pmic->gpioSetOutput(M5PM1_GPIO_NUM_3, M5PM1_GPIO_STATE_LOW);
        m5pm1_err_t shared_rail_result =
            s_hardware.pmic->gpioSetOutput(M5PM1_GPIO_NUM_2, M5PM1_GPIO_STATE_LOW);
        if ((speaker_result != M5PM1_OK && speaker_result != M5PM1_ERR_NOT_INIT) ||
            (shared_rail_result != M5PM1_OK && shared_rail_result != M5PM1_ERR_NOT_INIT)) {
            ESP_LOGW(TAG,
                     "event=hardware action=cleanup component=power_rails result=error "
                     "speaker_code=%d shared_code=%d",
                     static_cast<int>(speaker_result), static_cast<int>(shared_rail_result));
            result = -1;
        }
        delete s_hardware.pmic;
        s_hardware.pmic = nullptr;
    }
    if (s_hardware.bus) {
        esp_err_t err = i2c_del_master_bus(s_hardware.bus);
        if (err == ESP_OK) {
            s_hardware.bus = nullptr;
        } else {
            ESP_LOGW(TAG, "event=hardware action=cleanup component=i2c_bus result=error code=%s",
                     esp_err_to_name(err));
            result = -1;
        }
    }
    if (!s_hardware.pmic && !s_hardware.backlight_ready && !s_hardware.bus) {
        s_hardware = {};
    } else {
        s_hardware.initialized = false;
        s_hardware.speaker_powered = false;
    }
    return result;
}

extern "C" int mybot_sticks3_hardware_init(void) {
    if (s_hardware.initialized) {
        return 0;
    }
    if ((s_hardware.pmic || s_hardware.backlight_ready || s_hardware.bus) &&
        release_hardware() < 0) {
        ESP_LOGE(TAG, "event=hardware action=initialize result=error reason=cleanup_pending");
        return -1;
    }
    if (!s_hardware_mutex) {
        s_hardware_mutex = xSemaphoreCreateMutexStatic(&s_hardware_mutex_storage);
    }
    if (!s_hardware_mutex) {
        ESP_LOGE(TAG, "event=hardware action=initialize component=mutex result=error");
        return -1;
    }

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = MYBOT_STICKS3_I2C_PORT,
        .sda_io_num = MYBOT_STICKS3_I2C_SDA,
        .scl_io_num = MYBOT_STICKS3_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = 1,
                .allow_pd = 0,
            },
    };
    esp_err_t result = i2c_new_master_bus(&bus_config, &s_hardware.bus);
    if (result == ESP_OK) {
        result = initialize_backlight();
    }
    m5pm1_err_t pmic_result = M5PM1_OK;
    if (result == ESP_OK) {
        pmic_result = initialize_pmic();
    }
    if (result != ESP_OK || pmic_result != M5PM1_OK) {
        ESP_LOGE(TAG, "event=hardware action=initialize result=error component=%s code=%d",
                 result != ESP_OK ? "esp_driver" : "m5pm1",
                 result != ESP_OK ? static_cast<int>(result) : static_cast<int>(pmic_result));
        if (release_hardware() < 0) {
            ESP_LOGE(TAG, "event=hardware action=initialize cleanup=error");
        }
        return -1;
    }

    s_hardware.speaker_powered = false;
    s_hardware.initialized = true;
    ESP_LOGI(TAG,
             "event=hardware action=initialize result=ok i2c_port=%d pmic=0x%02x "
             "power=lcd_codec speaker=off",
             MYBOT_STICKS3_I2C_PORT, MYBOT_STICKS3_M5PM1_ADDRESS);
    return 0;
}

extern "C" int mybot_sticks3_hardware_deinit(void) {
    if (!s_hardware.pmic && !s_hardware.backlight_ready && !s_hardware.bus) {
        return 0;
    }
    if (!s_hardware_mutex || xSemaphoreTake(s_hardware_mutex, portMAX_DELAY) != pdTRUE) {
        return -1;
    }
    int result = release_hardware();
    xSemaphoreGive(s_hardware_mutex);
    ESP_LOGI(TAG, "event=hardware action=rollback result=%s", result == 0 ? "ok" : "error");
    return result;
}

extern "C" i2c_master_bus_handle_t mybot_sticks3_i2c_bus_handle(void) {
    return s_hardware.initialized ? s_hardware.bus : nullptr;
}

extern "C" int mybot_sticks3_set_speaker_power(bool enabled) {
    if (!s_hardware.initialized || !s_hardware.pmic || !s_hardware_mutex ||
        xSemaphoreTake(s_hardware_mutex, portMAX_DELAY) != pdTRUE) {
        return -1;
    }

    m5pm1_err_t result = M5PM1_OK;
    if (s_hardware.speaker_powered != enabled) {
        result = s_hardware.pmic->gpioSetOutput(M5PM1_GPIO_NUM_3, enabled ? M5PM1_GPIO_STATE_HIGH
                                                                          : M5PM1_GPIO_STATE_LOW);
        if (result == M5PM1_OK) {
            s_hardware.speaker_powered = enabled;
        }
    }
    xSemaphoreGive(s_hardware_mutex);

    if (result != M5PM1_OK) {
        ESP_LOGE(TAG, "event=power component=speaker enabled=%d result=error code=%d",
                 enabled ? 1 : 0, static_cast<int>(result));
        return -1;
    }
    ESP_LOGI(TAG, "event=power component=speaker enabled=%d result=ok", enabled ? 1 : 0);
    return 0;
}

extern "C" int mybot_sticks3_set_display_backlight(unsigned int percent) {
    if (percent > 100 || !s_hardware.initialized || !s_hardware.backlight_ready ||
        !s_hardware_mutex || xSemaphoreTake(s_hardware_mutex, portMAX_DELAY) != pdTRUE) {
        return -1;
    }

    uint32_t duty = (BACKLIGHT_MAX_DUTY * percent + 50U) / 100U;
    esp_err_t result = ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty);
    if (result == ESP_OK) {
        result = ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL);
    }
    xSemaphoreGive(s_hardware_mutex);

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "event=backlight level=%u result=error code=%s", percent,
                 esp_err_to_name(result));
        return -1;
    }
    ESP_LOGI(TAG, "event=backlight level=%u result=ok", percent);
    return 0;
}
