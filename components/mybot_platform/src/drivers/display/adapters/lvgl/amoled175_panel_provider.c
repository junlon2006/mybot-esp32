/* SPDX-License-Identifier: MIT */
/* Board-selected CO5300 panel provider for the shared LVGL adapter. */
#include "display/display_panel.h"

#include <stddef.h>

int mybot_amoled175_lvgl_panel_open(mybot_display_panel_t *panel,
                                    esp_lcd_panel_io_color_trans_done_cb_t callback,
                                    void *user_data);
int mybot_amoled175_lvgl_panel_close(mybot_display_panel_t *panel);

int mybot_display_panel_open(mybot_display_panel_t *panel) {
    return mybot_amoled175_lvgl_panel_open(panel, NULL, NULL);
}

int mybot_display_panel_close(mybot_display_panel_t *panel) {
    return mybot_amoled175_lvgl_panel_close(panel);
}
