/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_LVGL_ASSETS_H_
#define MYBOT_LVGL_ASSETS_H_

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MYBOT_UI_EMOJI_NEUTRAL,
    MYBOT_UI_EMOJI_HAPPY,
    MYBOT_UI_EMOJI_RELAXED,
    MYBOT_UI_EMOJI_THINKING,
    MYBOT_UI_EMOJI_COUNT,
} mybot_ui_emoji_t;

/* Constant 64x64 ARGB8888 images, straight alpha, stored as BGRA bytes.
 * The returned descriptor remains valid throughout firmware lifetime.
 * Invalid values select the neutral image. No allocation or decode occurs. */
const lv_image_dsc_t *mybot_lvgl_ui_emoji(mybot_ui_emoji_t emoji);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_LVGL_ASSETS_H_ */
