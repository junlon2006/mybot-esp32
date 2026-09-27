/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#include "board_config.h"
#include "display/board_backlight.h"
#include "display/display_panel.h"

#include "driver/spi_master.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#include <stddef.h>
#include <stdint.h>

#define TAG "st7789_panel"

int mybot_display_panel_close(mybot_display_panel_t *panel) {
    if (!panel) {
        return -1;
    }
    if (panel->backlight_ready) {
        if (mybot_board_set_display_backlight(0) < 0) {
            return -1;
        }
        panel->backlight_ready = false;
    }
    if (panel->panel) {
        if (panel->ready && esp_lcd_panel_disp_on_off(panel->panel, false) != ESP_OK) {
            ESP_LOGW(TAG, "event=panel action=display_off result=error");
        }
        if (esp_lcd_panel_del(panel->panel) != ESP_OK) {
            return -1;
        }
        panel->panel = NULL;
    }
    if (panel->io) {
        /* The LVGL adapter drains transfers before close; IO deletion joins callbacks. */
        if (esp_lcd_panel_io_del(panel->io) != ESP_OK) {
            return -1;
        }
        panel->io = NULL;
    }
    if (panel->spi_ready) {
        if (spi_bus_free(MYBOT_DISPLAY_SPI_HOST) != ESP_OK) {
            return -1;
        }
        panel->spi_ready = false;
    }
    panel->ready = false;
    panel->x_alignment = 0;
    panel->y_alignment = 0;
    return 0;
}

int mybot_display_panel_open(mybot_display_panel_t *panel) {
    if (!panel || panel->ready || mybot_display_panel_close(panel) < 0) {
        return -1;
    }
    if (mybot_board_set_display_backlight(0) < 0) {
        return -1;
    }
    panel->backlight_ready = true;

    const spi_bus_config_t bus = {
        .mosi_io_num = MYBOT_DISPLAY_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = MYBOT_DISPLAY_SCLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .data4_io_num = GPIO_NUM_NC,
        .data5_io_num = GPIO_NUM_NC,
        .data6_io_num = GPIO_NUM_NC,
        .data7_io_num = GPIO_NUM_NC,
        .max_transfer_sz = MYBOT_DISPLAY_WIDTH * MYBOT_DISPLAY_TRANSFER_ROWS * sizeof(uint16_t),
        .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
    };
    esp_err_t result = spi_bus_initialize(MYBOT_DISPLAY_SPI_HOST, &bus, SPI_DMA_CH_AUTO);
    if (result != ESP_OK) {
        goto fail;
    }
    panel->spi_ready = true;

    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = MYBOT_DISPLAY_CS,
        .dc_gpio_num = MYBOT_DISPLAY_DC,
        .spi_mode = MYBOT_DISPLAY_SPI_MODE,
        .pclk_hz = MYBOT_DISPLAY_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 2,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    result = esp_lcd_new_panel_io_spi(MYBOT_DISPLAY_SPI_HOST, &io_config, &panel->io);
    if (result != ESP_OK) {
        goto fail;
    }

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = MYBOT_DISPLAY_RESET,
        .rgb_ele_order = MYBOT_DISPLAY_RGB_ORDER,
        .bits_per_pixel = 16,
    };
    result = esp_lcd_new_panel_st7789(panel->io, &panel_config, &panel->panel);
    if (result == ESP_OK) {
        result = esp_lcd_panel_reset(panel->panel);
    }
    if (result == ESP_OK) {
        result = esp_lcd_panel_init(panel->panel);
    }
    if (result == ESP_OK) {
        result =
            esp_lcd_panel_set_gap(panel->panel, MYBOT_DISPLAY_OFFSET_X, MYBOT_DISPLAY_OFFSET_Y);
    }
    if (result == ESP_OK) {
        result = esp_lcd_panel_invert_color(panel->panel, MYBOT_DISPLAY_INVERT_COLOR);
    }
    if (result == ESP_OK) {
        result = esp_lcd_panel_swap_xy(panel->panel, MYBOT_DISPLAY_SWAP_XY);
    }
    if (result == ESP_OK) {
        result = esp_lcd_panel_mirror(panel->panel, MYBOT_DISPLAY_MIRROR_X, MYBOT_DISPLAY_MIRROR_Y);
    }
    if (result == ESP_OK) {
        result = esp_lcd_panel_disp_on_off(panel->panel, true);
    }
    if (result != ESP_OK) {
        goto fail;
    }
    panel->ready = true;
    if (mybot_board_set_display_backlight(MYBOT_DISPLAY_BRIGHTNESS_PERCENT) < 0) {
        (void)mybot_display_panel_close(panel);
        return -1;
    }
    return 0;

fail:
    ESP_LOGE(TAG, "event=panel action=initialize controller=st7789 result=error error=%s",
             esp_err_to_name(result));
    (void)mybot_display_panel_close(panel);
    return -1;
}
