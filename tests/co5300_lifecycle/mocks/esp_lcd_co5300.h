/* SPDX-License-Identifier: MIT */
#ifndef CO5300_HOST_DRIVER_H_
#define CO5300_HOST_DRIVER_H_

#include "host_platform.h"

typedef struct {
    int cmd;
    const void *data;
    size_t data_bytes;
    unsigned int delay_ms;
} co5300_lcd_init_cmd_t;
typedef struct {
    const co5300_lcd_init_cmd_t *init_cmds;
    size_t init_cmds_size;
    struct {
        bool use_qspi_interface;
    } flags;
} co5300_vendor_config_t;

esp_err_t esp_lcd_new_panel_co5300(esp_lcd_panel_io_handle_t io,
                                   const esp_lcd_panel_dev_config_t *config,
                                   esp_lcd_panel_handle_t *out);
esp_err_t esp_lcd_panel_co5300_set_brightness(esp_lcd_panel_handle_t panel, unsigned int percent);

#endif /* CO5300_HOST_DRIVER_H_ */
