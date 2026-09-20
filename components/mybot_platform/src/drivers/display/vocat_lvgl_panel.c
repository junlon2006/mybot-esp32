/* SPDX-License-Identifier: MIT */
/* VoCat LVGL panel ownership over the existing ST77916 driver. */
#include "dynamic_lcd_panel.h"
#include "vocat_st77916_lcd.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"

static void *s_legacy_context;

int mybot_vocat_lvgl_panel_open(mybot_dynamic_lcd_panel_t *panel,
                                esp_lcd_panel_io_color_trans_done_cb_t callback, void *user_data) {
    if (!panel || panel->ready || s_legacy_context) {
        return -1;
    }
    const mybot_lcd_ops_t *ops = mybot_vocat_lcd_ops();
    if (!ops || ops->init(&s_legacy_context) < 0) {
        s_legacy_context = NULL;
        return -1;
    }
    panel->io = mybot_vocat_lcd_panel_io(s_legacy_context);
    panel->panel = mybot_vocat_lcd_panel(s_legacy_context);
    if (!panel->io || !panel->panel) {
        ops->destroy(s_legacy_context);
        s_legacy_context = NULL;
        panel->io = NULL;
        panel->panel = NULL;
        return -1;
    }
    if (callback) {
        const esp_lcd_panel_io_callbacks_t callbacks = {
            .on_color_trans_done = callback,
        };
        if (esp_lcd_panel_io_register_event_callbacks(panel->io, &callbacks, user_data) != ESP_OK) {
            ops->destroy(s_legacy_context);
            s_legacy_context = NULL;
            panel->io = NULL;
            panel->panel = NULL;
            return -1;
        }
    }
    if (gpio_set_level(MYBOT_VOCAT_LCD_BACKLIGHT, 1) != ESP_OK) {
        ops->destroy(s_legacy_context);
        s_legacy_context = NULL;
        panel->io = NULL;
        panel->panel = NULL;
        return -1;
    }
    panel->ready = true;
    return 0;
}

int mybot_vocat_lvgl_panel_close(mybot_dynamic_lcd_panel_t *panel) {
    if (!panel || !panel->ready || !s_legacy_context) {
        return panel && !panel->ready ? 0 : -1;
    }
    const mybot_lcd_ops_t *ops = mybot_vocat_lcd_ops();
    ops->destroy(s_legacy_context);
    /* The legacy driver retains its context on a transport release failure. */
    if (mybot_vocat_lcd_panel_io(s_legacy_context) || mybot_vocat_lcd_panel(s_legacy_context)) {
        return -1;
    }
    s_legacy_context = NULL;
    panel->io = NULL;
    panel->panel = NULL;
    panel->ready = false;
    return 0;
}
