/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Project Contributors */
#include "atom_echos3r_hardware.h"
#include "board_config.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#define TAG "atom_echos3r_hw"

static i2c_master_bus_handle_t s_i2c_bus;
static bool s_pa_configured;
static bool s_ready;

static int release_hardware(void) {
    int result = 0;
    s_ready = false;

    if (s_pa_configured) {
        esp_err_t level_result = gpio_set_level(MYBOT_ATOM_ECHOS3R_PA_ENABLE, 0);
        esp_err_t reset_result = gpio_reset_pin(MYBOT_ATOM_ECHOS3R_PA_ENABLE);
        if (level_result == ESP_OK && reset_result == ESP_OK) {
            s_pa_configured = false;
        } else {
            ESP_LOGW(TAG, "event=hardware action=cleanup component=speaker_power result=error");
            result = -1;
        }
    }
    if (s_i2c_bus) {
        esp_err_t err = i2c_del_master_bus(s_i2c_bus);
        if (err == ESP_OK) {
            s_i2c_bus = NULL;
        } else {
            ESP_LOGW(TAG,
                     "event=hardware action=cleanup component=i2c_bus result=error "
                     "error=%s",
                     esp_err_to_name(err));
            result = -1;
        }
    }
    return result;
}

int mybot_atom_echos3r_hardware_init(void) {
    if (s_ready) {
        return 0;
    }
    if ((s_i2c_bus || s_pa_configured) && release_hardware() < 0) {
        ESP_LOGE(TAG, "event=hardware action=initialize result=error reason=cleanup_pending");
        return -1;
    }

    esp_err_t err = gpio_set_level(MYBOT_ATOM_ECHOS3R_PA_ENABLE, 0);
    if (err != ESP_OK) {
        goto fail;
    }
    const gpio_config_t pa_config = {
        .pin_bit_mask = 1ULL << MYBOT_ATOM_ECHOS3R_PA_ENABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&pa_config);
    if (err != ESP_OK) {
        goto fail;
    }
    s_pa_configured = true;

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = MYBOT_ATOM_ECHOS3R_I2C_PORT,
        .sda_io_num = MYBOT_ATOM_ECHOS3R_I2C_SDA,
        .scl_io_num = MYBOT_ATOM_ECHOS3R_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags =
            {
                .enable_internal_pullup = 1,
            },
    };
    err = i2c_new_master_bus(&bus_config, &s_i2c_bus);
    if (err != ESP_OK) {
        goto fail;
    }

    s_ready = true;
    ESP_LOGI(TAG, "event=hardware action=initialize result=ok i2c_port=%d speaker=off",
             MYBOT_ATOM_ECHOS3R_I2C_PORT);
    return 0;

fail:
    ESP_LOGE(TAG, "event=hardware action=initialize result=error error=%s", esp_err_to_name(err));
    if (release_hardware() < 0) {
        ESP_LOGE(TAG, "event=hardware action=initialize cleanup=error");
    }
    return -1;
}

int mybot_atom_echos3r_hardware_deinit(void) {
    int result = release_hardware();
    ESP_LOGI(TAG, "event=hardware action=rollback result=%s", result == 0 ? "ok" : "error");
    return result;
}

i2c_master_bus_handle_t mybot_atom_echos3r_i2c_bus_handle(void) {
    return s_ready ? s_i2c_bus : NULL;
}

int mybot_atom_echos3r_set_speaker_power(bool enabled) {
    if (!s_ready || !s_pa_configured) {
        return -1;
    }
    esp_err_t err = gpio_set_level(MYBOT_ATOM_ECHOS3R_PA_ENABLE, enabled ? 1 : 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "event=hardware component=speaker_power action=%s result=error error=%s",
                 enabled ? "enable" : "disable", esp_err_to_name(err));
        return -1;
    }
    return 0;
}
