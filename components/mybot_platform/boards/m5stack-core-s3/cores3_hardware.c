/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd. */
/* Copyright (c) 2025 Project Contributors */
#include "cores3_hardware.h"

#include "board_config.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdint.h>

#define TAG "cores3_hardware"
#define I2C_TIMEOUT_MS 100

typedef struct {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t axp2101;
    i2c_master_dev_handle_t aw9523;
    bool initialized;
} cores3_hardware_t;

static cores3_hardware_t s_hardware;

static esp_err_t add_device(uint16_t address, i2c_master_dev_handle_t *out_device) {
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = MYBOT_CORES3_I2C_SPEED_HZ,
    };
    return i2c_master_bus_add_device(s_hardware.bus, &config, out_device);
}

static esp_err_t write_register(i2c_master_dev_handle_t device, uint8_t reg, uint8_t value) {
    const uint8_t command[] = {reg, value};
    return i2c_master_transmit(device, command, sizeof(command), I2C_TIMEOUT_MS);
}

static esp_err_t read_register(i2c_master_dev_handle_t device, uint8_t reg, uint8_t *value) {
    return i2c_master_transmit_receive(device, &reg, sizeof(reg), value, sizeof(*value),
                                       I2C_TIMEOUT_MS);
}

static esp_err_t initialize_axp2101(void) {
    uint8_t value = 0;
    esp_err_t result = read_register(s_hardware.axp2101, 0x90, &value);
    if (result != ESP_OK) {
        return result;
    }

    const uint8_t registers[][2] = {
        {0x90, (uint8_t)(value | 0xb4)},
        {0x99, 0x19},
        {0x97, 0x1b},
        {0x69, 0x35},
        {0x30, 0x3f},
        {0x90, 0xbf},
        {0x94, 0x1c},
        {0x95, 0x1c},
    };
    for (size_t i = 0; i < sizeof(registers) / sizeof(registers[0]); ++i) {
        result = write_register(s_hardware.axp2101, registers[i][0], registers[i][1]);
        if (result != ESP_OK) {
            return result;
        }
    }
    return ESP_OK;
}

static esp_err_t initialize_aw9523(void) {
    static const uint8_t registers[][2] = {
        {0x02, 0x07}, {0x03, 0x8f}, {0x04, 0x18}, {0x05, 0x0c},
        {0x11, 0x10}, {0x12, 0xff}, {0x13, 0xff},
    };
    for (size_t i = 0; i < sizeof(registers) / sizeof(registers[0]); ++i) {
        esp_err_t result = write_register(s_hardware.aw9523, registers[i][0], registers[i][1]);
        if (result != ESP_OK) {
            return result;
        }
    }
    return ESP_OK;
}

int mybot_cores3_reset_audio_codec(void) {
    if (!s_hardware.aw9523) {
        return -1;
    }
    if (write_register(s_hardware.aw9523, 0x02, 0x03) != ESP_OK) {
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    if (write_register(s_hardware.aw9523, 0x02, 0x07) != ESP_OK) {
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "event=hardware component=aw88298 action=reset result=ok");
    return 0;
}

int mybot_cores3_reset_display(void) {
    if (!s_hardware.aw9523) {
        return -1;
    }
    if (write_register(s_hardware.aw9523, 0x03, 0x81) != ESP_OK) {
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    if (write_register(s_hardware.aw9523, 0x03, 0x83) != ESP_OK) {
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG, "event=hardware component=ili9342 action=reset result=ok");
    return 0;
}

int mybot_cores3_set_display_backlight(unsigned int percent) {
    if (!s_hardware.axp2101 || percent > 100) {
        return -1;
    }
    const uint8_t level = (uint8_t)((percent + 641U) >> 5);
    return write_register(s_hardware.axp2101, 0x99, level) == ESP_OK ? 0 : -1;
}

static void release_failed_initialization(void) {
    if (s_hardware.aw9523) {
        (void)i2c_master_bus_rm_device(s_hardware.aw9523);
    }
    if (s_hardware.axp2101) {
        (void)i2c_master_bus_rm_device(s_hardware.axp2101);
    }
    if (s_hardware.bus) {
        (void)i2c_del_master_bus(s_hardware.bus);
    }
    s_hardware = (cores3_hardware_t){0};
}

int mybot_cores3_hardware_init(void) {
    if (s_hardware.initialized) {
        return 0;
    }

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = MYBOT_CORES3_I2C_PORT,
        .sda_io_num = MYBOT_CORES3_I2C_SDA,
        .scl_io_num = MYBOT_CORES3_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t result = i2c_new_master_bus(&bus_config, &s_hardware.bus);
    if (result == ESP_OK) {
        result = add_device(MYBOT_CORES3_AXP2101_ADDRESS, &s_hardware.axp2101);
    }
    if (result == ESP_OK) {
        result = add_device(MYBOT_CORES3_AW9523_ADDRESS, &s_hardware.aw9523);
    }
    if (result == ESP_OK) {
        result = initialize_axp2101();
    }
    if (result == ESP_OK) {
        result = initialize_aw9523();
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "event=hardware action=initialize result=error code=%s",
                 esp_err_to_name(result));
        release_failed_initialization();
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    if (mybot_cores3_reset_audio_codec() < 0) {
        ESP_LOGE(TAG, "event=hardware component=aw88298 action=reset result=error");
        release_failed_initialization();
        return -1;
    }
    s_hardware.initialized = true;
    ESP_LOGI(TAG, "event=hardware action=initialize result=ok i2c_port=%d", MYBOT_CORES3_I2C_PORT);
    return 0;
}

i2c_master_bus_handle_t mybot_cores3_i2c_bus_handle(void) {
    return s_hardware.initialized ? s_hardware.bus : NULL;
}
