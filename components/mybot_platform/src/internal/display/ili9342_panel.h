/* SPDX-License-Identifier: MIT */
#ifndef MYBOT_ILI9342_PANEL_H_
#define MYBOT_ILI9342_PANEL_H_

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ILI9342_LCD_TRANSFER_ROWS 16

typedef struct {
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
    bool spi_ready;
} ili9342_panel_t;

int mybot_ili9342_panel_open(ili9342_panel_t *lcd, esp_lcd_panel_io_color_trans_done_cb_t done,
                             void *user);
/* Call after the renderer has stopped submitting transfers. Retains handles on failure. */
int mybot_ili9342_panel_close(ili9342_panel_t *lcd);

#ifdef __cplusplus
}
#endif

#endif
