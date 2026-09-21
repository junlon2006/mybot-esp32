/* SPDX-License-Identifier: MIT */
#ifndef MYBOT_DISPLAY_PANEL_H_
#define MYBOT_DISPLAY_PANEL_H_

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
    bool ready;
} mybot_display_panel_t;

/* Each board profile provides one implementation of this boundary. */
int mybot_display_panel_open(mybot_display_panel_t *panel);
int mybot_display_panel_close(mybot_display_panel_t *panel);

int mybot_amoled175_lvgl_panel_open(mybot_display_panel_t *panel,
                                    esp_lcd_panel_io_color_trans_done_cb_t callback,
                                    void *user_data);
int mybot_amoled175_lvgl_panel_close(mybot_display_panel_t *panel);

int mybot_sensecap_lvgl_panel_open(mybot_display_panel_t *panel,
                                   esp_lcd_panel_io_color_trans_done_cb_t callback,
                                   void *user_data);
int mybot_sensecap_lvgl_panel_close(mybot_display_panel_t *panel);

int mybot_vocat_lvgl_panel_open(mybot_display_panel_t *panel,
                                esp_lcd_panel_io_color_trans_done_cb_t callback, void *user_data);
int mybot_vocat_lvgl_panel_close(mybot_display_panel_t *panel);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_DISPLAY_PANEL_H_ */
