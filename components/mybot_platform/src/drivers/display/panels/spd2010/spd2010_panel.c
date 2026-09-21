/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#include "board_config.h"
#include "display/display_panel.h"
#include "sensecap_hardware.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_spd2010.h"
#include "esp_log.h"

#include <stddef.h>
#include <stdint.h>

#define TAG "spd2010_panel"

_Static_assert((MYBOT_DISPLAY_WIDTH % 4) == 0, "SPD2010 transfers require 4-pixel X alignment");

int mybot_display_panel_close(mybot_display_panel_t *panel) {
    if (!panel) {
        return -1;
    }
    if (panel->power_ready && mybot_sensecap_set_display_backlight(0) < 0) {
        ESP_LOGW(TAG, "event=panel action=backlight_off result=error");
    }
    if (panel->panel) {
        if (panel->ready && esp_lcd_panel_disp_on_off(panel->panel, false) != ESP_OK) {
            ESP_LOGW(TAG, "event=panel action=display_off result=error");
        }
        esp_err_t result = esp_lcd_panel_del(panel->panel);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "event=panel action=destroy component=panel result=error code=%s",
                     esp_err_to_name(result));
            return -1;
        }
        panel->panel = NULL;
    }
    if (panel->io) {
        /* The shared LVGL adapter drains its flush callback before closing.
         * IO deletion also waits for all queued SPI transfers and callbacks. */
        esp_err_t result = esp_lcd_panel_io_del(panel->io);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "event=panel action=destroy component=panel_io result=error code=%s",
                     esp_err_to_name(result));
            return -1;
        }
        panel->io = NULL;
    }
    if (panel->spi_ready) {
        esp_err_t result = spi_bus_free(MYBOT_SENSECAP_LCD_SPI_HOST);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "event=panel action=destroy component=spi_bus result=error code=%s",
                     esp_err_to_name(result));
            return -1;
        }
        panel->spi_ready = false;
    }
    if (panel->power_ready) {
        if (mybot_sensecap_set_lcd_power(false) < 0) {
            ESP_LOGE(TAG, "event=panel action=power_off result=error");
            return -1;
        }
        panel->power_ready = false;
    }
    panel->ready = false;
    panel->x_alignment = 0;
    panel->y_alignment = 0;
    return 0;
}

int mybot_display_panel_open(mybot_display_panel_t *panel) {
    if (!panel || panel->ready) {
        return -1;
    }
    if (mybot_display_panel_close(panel) < 0) {
        return -1;
    }
    if (mybot_sensecap_set_lcd_power(true) < 0) {
        return -1;
    }
    panel->power_ready = true;
    const spi_bus_config_t bus_config = {
        .sclk_io_num = MYBOT_DISPLAY_PCLK,
        .data0_io_num = MYBOT_DISPLAY_DATA0,
        .data1_io_num = MYBOT_DISPLAY_DATA1,
        .data2_io_num = MYBOT_DISPLAY_DATA2,
        .data3_io_num = MYBOT_DISPLAY_DATA3,
        .max_transfer_sz = MYBOT_DISPLAY_WIDTH * MYBOT_DISPLAY_TRANSFER_ROWS * sizeof(uint16_t),
    };
    if (spi_bus_initialize(MYBOT_SENSECAP_LCD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO) != ESP_OK) {
        (void)mybot_display_panel_close(panel);
        return -1;
    }
    panel->spi_ready = true;

    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = MYBOT_DISPLAY_CS,
        .dc_gpio_num = GPIO_NUM_NC,
        .spi_mode = 3,
        .pclk_hz = MYBOT_SENSECAP_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 2,
        .lcd_cmd_bits = MYBOT_SENSECAP_LCD_COMMAND_BITS,
        .lcd_param_bits = MYBOT_SENSECAP_LCD_PARAMETER_BITS,
        .flags.quad_mode = true,
    };
    if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)MYBOT_SENSECAP_LCD_SPI_HOST, &io_config,
                                 &panel->io) != ESP_OK) {
        (void)mybot_display_panel_close(panel);
        return -1;
    }

    const spd2010_vendor_config_t vendor_config = {
        .flags.use_qspi_interface = true,
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = MYBOT_SENSECAP_LCD_RESET,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = (void *)&vendor_config,
    };
    if (esp_lcd_new_panel_spd2010(panel->io, &panel_config, &panel->panel) != ESP_OK ||
        esp_lcd_panel_reset(panel->panel) != ESP_OK || esp_lcd_panel_init(panel->panel) != ESP_OK ||
        esp_lcd_panel_mirror(panel->panel, MYBOT_DISPLAY_MIRROR_X, MYBOT_DISPLAY_MIRROR_Y) !=
            ESP_OK ||
        esp_lcd_panel_disp_on_off(panel->panel, true) != ESP_OK ||
        mybot_sensecap_set_display_backlight(100) < 0) {
        if (mybot_display_panel_close(panel) < 0) {
            ESP_LOGE(TAG, "event=lcd action=initialize cleanup=error");
        }
        ESP_LOGE(TAG, "event=lcd action=initialize result=error");
        return -1;
    }

    panel->ready = true;
    panel->x_alignment = 4;
    panel->y_alignment = 1;
    return 0;
}
