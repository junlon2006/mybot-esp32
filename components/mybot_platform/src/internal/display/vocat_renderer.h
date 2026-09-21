/* SPDX-License-Identifier: MIT */
#ifndef MYBOT_VOCAT_RENDERER_H_
#define MYBOT_VOCAT_RENDERER_H_

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#include <mybot/platform/mybot_lcd.h>

#ifdef __cplusplus
extern "C" {
#endif

const mybot_lcd_ops_t *mybot_vocat_legacy_renderer_ops(void);
esp_lcd_panel_io_handle_t mybot_vocat_legacy_panel_io(void *context);
esp_lcd_panel_handle_t mybot_vocat_legacy_panel(void *context);

#ifdef __cplusplus
}
#endif /* MYBOT_VOCAT_RENDERER_H_ */

#endif
