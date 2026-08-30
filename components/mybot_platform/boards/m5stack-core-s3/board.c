/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_board.h"

#include <mybot/platform/mybot_platform.h>

#include "board_actions.h"
#include "board_config.h"
#include "cores3_hardware.h"
#include "embedded_ogg_prompt.h"
#include "esp_log.h"
#include "wifi_control.h"

#include <stdbool.h>

#define TAG "mybot_platform"

const mybot_audio_capture_ops_t *mybot_cores3_audio_capture_ops(void);
const mybot_audio_playback_ops_t *mybot_cores3_audio_playback_ops(void);
const mybot_audio_volume_ops_t *mybot_cores3_audio_volume_ops(void);
const mybot_announce_ops_t *mybot_esp32s3_announce_ops(void);
const mybot_key_ops_t *mybot_cores3_touch_ops(void);
const mybot_https_ops_t *mybot_esp32s3_https_ops(void);
const mybot_kv_store_ops_t *mybot_esp32s3_kv_store_ops(void);
const mybot_lcd_ops_t *mybot_cores3_lcd_ops(void);
const mybot_wifi_ops_t *mybot_esp32s3_wifi_ops(void);
int mybot_cores3_touch_start(void);

static const mybot_lcd_ops_t *s_lcd_ops;
static void *s_lcd_ctx;
static bool s_automatic_provisioning_started;

static int board_show_screen(mybot_lcd_screen_t screen) {
    if (!s_lcd_ops || !s_lcd_ctx) {
        return -1;
    }
    mybot_lcd_content_t content = {
        .screen = screen,
    };
    return s_lcd_ops->render(s_lcd_ctx, &content);
}

static void board_on_provisioning(const char *trigger) {
    if (board_show_screen(MYBOT_LCD_SCREEN_WIFI_PROVISIONING) < 0) {
        ESP_LOGW(TAG, "event=display screen=wifi_provisioning trigger=%s result=error", trigger);
    } else {
        ESP_LOGI(TAG, "event=display screen=wifi_provisioning trigger=%s result=ok", trigger);
    }
    (void)mybot_embedded_ogg_play_wifi_provisioning(mybot_cores3_audio_playback_ops(),
                                                    mybot_cores3_audio_volume_ops(), trigger);
}

static void board_on_automatic_provisioning(void) {
    s_automatic_provisioning_started = true;
    board_on_provisioning("automatic");
}

static void board_on_touch_provisioning(void) {
    board_on_provisioning("touch");
}

static int board_prepare(void) {
    if (mybot_cores3_hardware_init() < 0) {
        ESP_LOGE(TAG, "event=board_prepare component=hardware result=error");
        return -1;
    }
    s_lcd_ops = mybot_cores3_lcd_ops();
    if (!s_lcd_ops || s_lcd_ops->init(&s_lcd_ctx) < 0) {
        ESP_LOGE(TAG, "event=board_prepare component=display result=error");
        return -1;
    }
    if (board_show_screen(MYBOT_LCD_SCREEN_STARTING) < 0) {
        s_lcd_ops->destroy(s_lcd_ctx);
        s_lcd_ctx = NULL;
        ESP_LOGE(TAG, "event=board_prepare component=display result=error reason=render");
        return -1;
    }
    if (mybot_cores3_touch_start() < 0) {
        s_lcd_ops->destroy(s_lcd_ctx);
        s_lcd_ctx = NULL;
        ESP_LOGE(TAG, "event=board_prepare component=touch result=error");
        return -1;
    }
    return 0;
}

int mybot_board_handle_boot_long_press(void) {
    mybot_board_request_wifi_provisioning();
    return 0;
}

static int board_provision_wifi(void) {
    return mybot_wifi_run_provisioning(board_on_touch_provisioning);
}

static int board_ensure_network(const char *device_id) {
    s_automatic_provisioning_started = false;
    int result = mybot_wifi_ensure_network(device_id, board_on_automatic_provisioning);
    if (s_automatic_provisioning_started) {
        (void)mybot_board_wait_wifi_provisioning_request(0);
    }
    return result;
}

static void board_shutdown_network(void) {
    mybot_wifi_shutdown_network();
}

static int board_register_platform(void) {
    const mybot_platform_descriptor_t descriptor = {
        .wifi = mybot_esp32s3_wifi_ops(),
        .kv_store = mybot_esp32s3_kv_store_ops(),
        .key = mybot_cores3_touch_ops(),
        .audio_capture = mybot_cores3_audio_capture_ops(),
        .audio_playback = mybot_cores3_audio_playback_ops(),
        .audio_volume = mybot_cores3_audio_volume_ops(),
        .announce = mybot_esp32s3_announce_ops(),
        .https = mybot_esp32s3_https_ops(),
        .lcd = mybot_cores3_lcd_ops(),
    };
    return mybot_platform_register(&descriptor);
}

static const mybot_board_t s_board = {
    .id = MYBOT_BOARD_NAME,
    .hw_model = MYBOT_BOARD_NAME,
    .prepare = board_prepare,
    .register_platform = board_register_platform,
    .ensure_network = board_ensure_network,
    .provision_wifi = board_provision_wifi,
    .shutdown_network = board_shutdown_network,
};

const mybot_board_t *mybot_board_get(void) {
    return &s_board;
}
