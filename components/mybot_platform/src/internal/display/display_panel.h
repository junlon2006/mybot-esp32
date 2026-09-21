/* SPDX-License-Identifier: MIT */
#ifndef MYBOT_DISPLAY_PANEL_H_
#define MYBOT_DISPLAY_PANEL_H_

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MYBOT_LVGL_TRANSFER_ROWS
#define MYBOT_LVGL_TRANSFER_ROWS 16
#endif
#define MYBOT_DISPLAY_TRANSFER_ROWS MYBOT_LVGL_TRANSFER_ROWS

typedef struct {
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
    bool spi_ready;
    bool backlight_ready;
    bool power_ready;
    bool ready;
    /* Pixel boundary granularity; 0/1 permits arbitrary rectangles. */
    uint8_t x_alignment;
    uint8_t y_alignment;
} mybot_display_panel_t;

/* Each board provides one implementation. Acquired handles remain owned by this
 * object on any failure; close is idempotent and supports partial-cleanup retries.
 * The caller must stop rendering and drain callbacks before closing the panel. */
int mybot_display_panel_open(mybot_display_panel_t *panel);
int mybot_display_panel_close(mybot_display_panel_t *panel);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_DISPLAY_PANEL_H_ */
