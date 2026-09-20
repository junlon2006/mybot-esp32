/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#include "cores3_lcd_panel.h"

#include "board_config.h"
#include "cores3_hardware.h"
#include "driver/spi_master.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_io_spi.h"
#include "esp_log.h"

#define TAG "cores3_panel"

int mybot_cores3_lcd_panel_close(cores3_lcd_panel_t *lcd) {
    if (!lcd) {
        return -1;
    }
    if (lcd->panel) {
        if (esp_lcd_panel_del(lcd->panel) != ESP_OK) {
            return -1;
        }
        lcd->panel = NULL;
    }
    if (lcd->io) {
        /* The SPI panel IO destructor waits for its queued transfers/callbacks. */
        if (esp_lcd_panel_io_del(lcd->io) != ESP_OK) {
            return -1;
        }
        lcd->io = NULL;
    }
    if (lcd->spi_ready) {
        if (spi_bus_free(SPI3_HOST) != ESP_OK) {
            return -1;
        }
        lcd->spi_ready = false;
    }
    return 0;
}

int mybot_cores3_lcd_panel_open(cores3_lcd_panel_t *lcd,
                                esp_lcd_panel_io_color_trans_done_cb_t done, void *user) {
    if (!lcd || lcd->spi_ready || lcd->io || lcd->panel || !mybot_cores3_i2c_bus_handle()) {
        return -1;
    }
    const spi_bus_config_t bus_config = {
        .mosi_io_num = MYBOT_DISPLAY_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = MYBOT_DISPLAY_SCLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = MYBOT_DISPLAY_WIDTH * CORES3_LCD_TRANSFER_ROWS * sizeof(uint16_t),
    };
    if (spi_bus_initialize(SPI3_HOST, &bus_config, SPI_DMA_CH_AUTO) != ESP_OK) {
        return -1;
    }
    lcd->spi_ready = true;
    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = MYBOT_DISPLAY_CS,
        .dc_gpio_num = MYBOT_DISPLAY_DC,
        .spi_mode = 2,
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 1,
        .on_color_trans_done = done,
        .user_ctx = user,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    if (esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &lcd->io) != ESP_OK) {
        goto failed;
    }
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_NC,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    if (esp_lcd_new_panel_ili9341(lcd->io, &panel_config, &lcd->panel) != ESP_OK ||
        esp_lcd_panel_reset(lcd->panel) != ESP_OK || mybot_cores3_reset_display() < 0 ||
        esp_lcd_panel_init(lcd->panel) != ESP_OK ||
        esp_lcd_panel_invert_color(lcd->panel, true) != ESP_OK ||
        esp_lcd_panel_disp_on_off(lcd->panel, true) != ESP_OK ||
        mybot_cores3_set_display_backlight(100) < 0) {
        goto failed;
    }
    return 0;
failed:
    if (mybot_cores3_lcd_panel_close(lcd) < 0) {
        ESP_LOGE(TAG, "event=panel action=cleanup result=error resources=retained");
    }
    return -1;
}
