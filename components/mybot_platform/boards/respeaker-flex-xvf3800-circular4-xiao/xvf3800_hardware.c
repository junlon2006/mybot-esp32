/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd. */
/* Copyright (c) 2025 Project Contributors */
#include "xvf3800_hardware.h"

#include "board_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>

#define TAG "xvf3800_hardware"
#define I2C_TIMEOUT_MS 100
#define XVF3800_CONTROL_SUCCESS 0
#define XVF3800_SERVICER_RETRY 64
#define XVF3800_GPI_RESOURCE_ID 36
#define XVF3800_GPI_READ_VALUES_COMMAND 0

typedef struct {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t aic3104;
    i2c_master_dev_handle_t xvf3800;
    bool initialized;
} xvf3800_hardware_t;

static xvf3800_hardware_t s_hardware;

static esp_err_t add_device(uint16_t address, i2c_master_dev_handle_t *out_device) {
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = MYBOT_XVF3800_I2C_SPEED_HZ,
    };
    return i2c_master_bus_add_device(s_hardware.bus, &config, out_device);
}

static esp_err_t write_register(i2c_master_dev_handle_t device, uint8_t reg, uint8_t value) {
    const uint8_t command[] = {reg, value};
    return i2c_master_transmit(device, command, sizeof(command), I2C_TIMEOUT_MS);
}

static esp_err_t initialize_aic3104(void) {
    static const uint8_t registers[][2] = {
        {0x00, 0x00}, /* Page 0. */
        {0x2b, 0x00}, /* Left DAC, 0 dB. */
        {0x2c, 0x00}, /* Right DAC, 0 dB. */
        {0x33, 0x0d}, /* HPLOUT powered and unmuted. */
        {0x41, 0x0d}, /* HPROUT powered and unmuted. */
        {0x56, 0x0b}, /* Left line out powered and unmuted. */
        {0x5d, 0x0b}, /* Right line out powered and unmuted. */
    };

    for (size_t i = 0; i < sizeof(registers) / sizeof(registers[0]); ++i) {
        esp_err_t result = write_register(s_hardware.aic3104, registers[i][0], registers[i][1]);
        if (result != ESP_OK) {
            ESP_LOGE(
                TAG,
                "event=hardware component=aic3104 action=initialize result=error register=0x%02x "
                "code=%s",
                registers[i][0], esp_err_to_name(result));
            return result;
        }
    }
    ESP_LOGI(TAG, "event=hardware component=aic3104 action=initialize result=ok address=0x%02x",
             MYBOT_AIC3104_I2C_ADDRESS);
    return ESP_OK;
}

static esp_err_t initialize_xvf3800_control(void) {
    const uint8_t reset_command[] = {0x00, 0x00, 0x00};
    esp_err_t result = i2c_master_transmit(s_hardware.xvf3800, reset_command, sizeof(reset_command),
                                           I2C_TIMEOUT_MS);
    if (result != ESP_OK) {
        return result;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG,
             "event=hardware component=xvf3800 action=control_command result=sent address=0x%02x",
             MYBOT_XVF3800_I2C_ADDRESS);
    return ESP_OK;
}

static void release_failed_initialization(void) {
    if (s_hardware.xvf3800) {
        (void)i2c_master_bus_rm_device(s_hardware.xvf3800);
    }
    if (s_hardware.aic3104) {
        (void)i2c_master_bus_rm_device(s_hardware.aic3104);
    }
    if (s_hardware.bus) {
        (void)i2c_del_master_bus(s_hardware.bus);
    }
    s_hardware = (xvf3800_hardware_t){0};
}

int mybot_xvf3800_hardware_init(void) {
    if (s_hardware.initialized) {
        return 0;
    }

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = MYBOT_XVF3800_I2C_PORT,
        .sda_io_num = MYBOT_XVF3800_I2C_SDA,
        .scl_io_num = MYBOT_XVF3800_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t result = i2c_new_master_bus(&bus_config, &s_hardware.bus);
    if (result == ESP_OK) {
        result = add_device(MYBOT_AIC3104_I2C_ADDRESS, &s_hardware.aic3104);
    }
    if (result == ESP_OK) {
        result = add_device(MYBOT_XVF3800_I2C_ADDRESS, &s_hardware.xvf3800);
    }
    if (result == ESP_OK) {
        result = initialize_aic3104();
    }
    if (result == ESP_OK) {
        result = initialize_xvf3800_control();
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "event=hardware action=initialize result=error code=%s",
                 esp_err_to_name(result));
        release_failed_initialization();
        return -1;
    }

    s_hardware.initialized = true;
    ESP_LOGI(TAG, "event=hardware action=initialize result=ok i2c_port=%d sda=%d scl=%d",
             MYBOT_XVF3800_I2C_PORT, MYBOT_XVF3800_I2C_SDA, MYBOT_XVF3800_I2C_SCL);
    return 0;
}

esp_err_t mybot_xvf3800_read_button(bool *released) {
    if (!released || !s_hardware.initialized || !s_hardware.xvf3800) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t command[] = {
        XVF3800_GPI_RESOURCE_ID,
        (uint8_t)(0x80 | XVF3800_GPI_READ_VALUES_COMMAND),
        4,
    };
    uint8_t response[4] = {0};
    esp_err_t result = i2c_master_transmit_receive(s_hardware.xvf3800, command, sizeof(command),
                                                   response, sizeof(response), I2C_TIMEOUT_MS);
    if (result != ESP_OK) {
        return result;
    }
    if (response[0] == XVF3800_SERVICER_RETRY) {
        return ESP_ERR_NOT_FINISHED;
    }
    if (response[0] != XVF3800_CONTROL_SUCCESS) {
        ESP_LOGW(TAG, "event=button source=xvf3800_gpi0 action=read result=error status=%u",
                 response[0]);
        return ESP_ERR_INVALID_RESPONSE;
    }

    *released = response[1] != 0;
    return ESP_OK;
}
