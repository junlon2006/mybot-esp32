/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_board.h"

#include <mybot/platform/mybot_platform.h>

#include "board_config.h"
#include "esp_log.h"

#define TAG "mybot_platform"

const mybot_audio_capture_ops_t *mybot_esp32s3_audio_capture_ops(void);
const mybot_audio_playback_ops_t *mybot_esp32s3_audio_playback_ops(void);
const mybot_key_ops_t *mybot_esp32s3_button_ops(void);
const mybot_https_ops_t *mybot_esp32s3_https_ops(void);
const mybot_kv_store_ops_t *mybot_esp32s3_kv_store_ops(void);
const mybot_lcd_ops_t *mybot_esp32s3_lcd_ops(void);
const mybot_wifi_ops_t *mybot_esp32s3_wifi_ops(void);

static int board_prepare(void) {
    return 0;
}

static int board_register_platform(void) {
    const mybot_platform_descriptor_t descriptor = {
        .wifi = mybot_esp32s3_wifi_ops(),
        .kv_store = mybot_esp32s3_kv_store_ops(),
        .key = mybot_esp32s3_button_ops(),
        .audio_capture = mybot_esp32s3_audio_capture_ops(),
        .audio_playback = mybot_esp32s3_audio_playback_ops(),
        .https = mybot_esp32s3_https_ops(),
        .lcd = mybot_esp32s3_lcd_ops(),
    };

    int ret = mybot_platform_register(&descriptor);
    if (ret == 0) {
        ESP_LOGI(TAG, "zhengchen platform adapters registered");
    }
    return ret;
}

static const mybot_board_t s_board = {
    .id = MYBOT_BOARD_NAME,
    .hw_model = MYBOT_BOARD_NAME,
    .prepare = board_prepare,
    .register_platform = board_register_platform,
};

const mybot_board_t *mybot_board_get(void) {
    return &s_board;
}
