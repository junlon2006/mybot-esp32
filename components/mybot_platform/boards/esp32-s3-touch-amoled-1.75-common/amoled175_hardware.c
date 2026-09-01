/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#include "amoled175_hardware.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include <stddef.h>
#include <stdint.h>

#define TAG "amoled175_hw"
#define I2C_TIMEOUT_MS 100

typedef struct {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t axp2101;
    bool pa_ready;
    bool pa_enabled;
    bool initialized;
} amoled175_hardware_t;

static amoled175_hardware_t s_hardware;

static esp_err_t write_register(uint8_t reg, uint8_t value) {
    const uint8_t command[] = {reg, value};
    return i2c_master_transmit(s_hardware.axp2101, command, sizeof(command), I2C_TIMEOUT_MS);
}

static esp_err_t initialize_axp2101(void) {
    /* Keep the AMOLED 1.75 family reference power and charger sequence byte-for-byte. */
    static const uint8_t registers[][2] = {
        {0x22, 0x06}, {0x27, 0x10}, {0x80, 0x01}, {0x90, 0x00}, {0x91, 0x00}, {0x82, 0x12},
        {0x92, 0x1c}, {0x90, 0x01}, {0x64, 0x02}, {0x61, 0x02}, {0x62, 0x08}, {0x63, 0x01},
    };

    for (size_t index = 0; index < sizeof(registers) / sizeof(registers[0]); ++index) {
        esp_err_t result = write_register(registers[index][0], registers[index][1]);
        if (result != ESP_OK) {
            ESP_LOGE(TAG,
                     "event=power component=axp2101 action=configure result=error register=0x%02x "
                     "code=%s",
                     registers[index][0], esp_err_to_name(result));
            return result;
        }
    }
    return ESP_OK;
}

static esp_err_t initialize_pa(void) {
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << MYBOT_AMOLED175_SPEAKER_PA,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t result = gpio_config(&config);
    if (result == ESP_OK) {
        result = gpio_set_level(MYBOT_AMOLED175_SPEAKER_PA, 0);
    }
    if (result == ESP_OK) {
        s_hardware.pa_ready = true;
        s_hardware.pa_enabled = false;
    }
    return result;
}

int mybot_amoled175_hardware_deinit(void) {
    int result = 0;

    if (s_hardware.pa_ready) {
        esp_err_t err = gpio_set_level(MYBOT_AMOLED175_SPEAKER_PA, 0);
        if (err != ESP_OK) {
            ESP_LOGW(TAG,
                     "event=hardware action=deinitialize component=speaker_pa result=error "
                     "code=%s",
                     esp_err_to_name(err));
            result = -1;
        } else {
            s_hardware.pa_enabled = false;
        }
    }
    if (s_hardware.axp2101) {
        esp_err_t err = i2c_master_bus_rm_device(s_hardware.axp2101);
        if (err == ESP_OK) {
            s_hardware.axp2101 = NULL;
        } else {
            ESP_LOGW(TAG,
                     "event=hardware action=deinitialize component=axp2101 result=error "
                     "code=%s",
                     esp_err_to_name(err));
            result = -1;
        }
    }
    if (!s_hardware.axp2101 && s_hardware.bus) {
        esp_err_t err = i2c_del_master_bus(s_hardware.bus);
        if (err == ESP_OK) {
            s_hardware.bus = NULL;
        } else {
            ESP_LOGW(TAG,
                     "event=hardware action=deinitialize component=i2c_bus result=error "
                     "code=%s",
                     esp_err_to_name(err));
            result = -1;
        }
    }

    s_hardware.initialized = false;
    if (!s_hardware.axp2101 && !s_hardware.bus) {
        s_hardware = (amoled175_hardware_t){0};
    }
    ESP_LOGI(TAG, "event=hardware action=deinitialize result=%s", result == 0 ? "ok" : "error");
    return result;
}

int mybot_amoled175_hardware_init(void) {
    if (s_hardware.initialized) {
        return 0;
    }
    if ((s_hardware.bus || s_hardware.axp2101) && mybot_amoled175_hardware_deinit() < 0) {
        ESP_LOGE(TAG, "event=hardware action=initialize result=error reason=cleanup_pending");
        return -1;
    }

    esp_err_t result = initialize_pa();
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = MYBOT_AMOLED175_I2C_PORT,
        .sda_io_num = MYBOT_AMOLED175_I2C_SDA,
        .scl_io_num = MYBOT_AMOLED175_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (result == ESP_OK) {
        result = i2c_new_master_bus(&bus_config, &s_hardware.bus);
    }
    const i2c_device_config_t axp_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MYBOT_AMOLED175_AXP2101_ADDRESS,
        .scl_speed_hz = MYBOT_AMOLED175_I2C_SPEED_HZ,
    };
    if (result == ESP_OK) {
        result = i2c_master_bus_add_device(s_hardware.bus, &axp_config, &s_hardware.axp2101);
    }
    if (result == ESP_OK) {
        result = initialize_axp2101();
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "event=hardware action=initialize result=error code=%s",
                 esp_err_to_name(result));
        if (mybot_amoled175_hardware_deinit() < 0) {
            ESP_LOGE(TAG, "event=hardware action=initialize cleanup=error");
        }
        return -1;
    }

#if MYBOT_AMOLED175_HAS_TCA9554
    const esp_err_t probe =
        i2c_master_probe(s_hardware.bus, MYBOT_AMOLED175_TCA9554_ADDRESS, I2C_TIMEOUT_MS);
    if (probe == ESP_OK) {
        ESP_LOGI(TAG, "event=hardware component=tca9554 action=probe result=present address=0x%02x",
                 MYBOT_AMOLED175_TCA9554_ADDRESS);
    } else if (probe == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(
            TAG,
            "event=hardware component=tca9554 action=probe result=absent_optional address=0x%02x "
            "code=%s",
            MYBOT_AMOLED175_TCA9554_ADDRESS, esp_err_to_name(probe));
    } else {
        ESP_LOGE(TAG,
                 "event=hardware component=tca9554 action=probe result=error address=0x%02x "
                 "code=%s",
                 MYBOT_AMOLED175_TCA9554_ADDRESS, esp_err_to_name(probe));
        if (mybot_amoled175_hardware_deinit() < 0) {
            ESP_LOGE(TAG, "event=hardware action=initialize cleanup=error");
        }
        return -1;
    }
#endif

    s_hardware.initialized = true;
    ESP_LOGI(TAG,
             "event=hardware action=initialize result=ok "
             "board_variant=" MYBOT_AMOLED175_BOARD_VARIANT " i2c_port=%d axp2101=0x%02x",
             MYBOT_AMOLED175_I2C_PORT, MYBOT_AMOLED175_AXP2101_ADDRESS);
    return 0;
}

i2c_master_bus_handle_t mybot_amoled175_i2c_bus_handle(void) {
    return s_hardware.initialized ? s_hardware.bus : NULL;
}

int mybot_amoled175_set_speaker_pa(bool enabled) {
    if (!s_hardware.initialized || !s_hardware.pa_ready) {
        return -1;
    }
    if (s_hardware.pa_enabled == enabled) {
        return 0;
    }
    esp_err_t result = gpio_set_level(MYBOT_AMOLED175_SPEAKER_PA, enabled ? 1 : 0);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "event=power component=speaker_pa enabled=%d result=error code=%s",
                 enabled ? 1 : 0, esp_err_to_name(result));
        return -1;
    }
    s_hardware.pa_enabled = enabled;
    ESP_LOGI(TAG, "event=power component=speaker_pa enabled=%d result=ok", enabled ? 1 : 0);
    return 0;
}
