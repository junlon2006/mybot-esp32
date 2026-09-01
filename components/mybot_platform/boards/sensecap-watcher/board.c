/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#include "mybot_board.h"

#include <mybot/platform/mybot_platform.h>

#include "board_actions.h"
#include "board_config.h"
#include "embedded_ogg_prompt.h"
#include "esp_log.h"
#include "sensecap_hardware.h"
#include "wifi_control.h"

#include <stdbool.h>

#define TAG "mybot_platform"

const mybot_audio_capture_ops_t *mybot_sensecap_audio_capture_ops(void);
const mybot_audio_playback_ops_t *mybot_sensecap_audio_playback_ops(void);
const mybot_audio_volume_ops_t *mybot_sensecap_audio_volume_ops(void);
const mybot_announce_ops_t *mybot_esp32s3_announce_ops(void);
const mybot_https_ops_t *mybot_esp32s3_https_ops(void);
const mybot_key_ops_t *mybot_sensecap_input_ops(void);
const mybot_kv_store_ops_t *mybot_esp32s3_kv_store_ops(void);
const mybot_lcd_ops_t *mybot_sensecap_lcd_ops(void);
const mybot_wifi_ops_t *mybot_esp32s3_wifi_ops(void);
int mybot_sensecap_input_start(void);

static const mybot_lcd_ops_t *s_lcd_ops;
static void *s_lcd_context;
static bool s_automatic_provisioning_started;

static int board_show_screen(mybot_lcd_screen_t screen) {
    if (!s_lcd_ops || !s_lcd_context) {
        return -1;
    }
    const mybot_lcd_content_t content = {
        .screen = screen,
    };
    return s_lcd_ops->render(s_lcd_context, &content);
}

static void board_on_provisioning(const char *trigger) {
    if (board_show_screen(MYBOT_LCD_SCREEN_WIFI_PROVISIONING) < 0) {
        ESP_LOGW(TAG, "event=display screen=wifi_provisioning trigger=%s result=error", trigger);
    } else {
        ESP_LOGI(TAG, "event=display screen=wifi_provisioning trigger=%s result=ok", trigger);
    }
    (void)mybot_embedded_ogg_play_wifi_provisioning(mybot_sensecap_audio_playback_ops(),
                                                    mybot_sensecap_audio_volume_ops(), trigger);
}

static void board_on_automatic_provisioning(void) {
    s_automatic_provisioning_started = true;
    board_on_provisioning("automatic");
}

static void board_on_button_provisioning(void) {
    board_on_provisioning("button");
}

static int board_prepare(void) {
    if (mybot_sensecap_hardware_init() < 0) {
        ESP_LOGE(TAG, "event=board_prepare component=hardware result=error");
        return -1;
    }

    s_lcd_ops = mybot_sensecap_lcd_ops();
    if (!s_lcd_ops || s_lcd_ops->init(&s_lcd_context) < 0) {
        ESP_LOGE(TAG, "event=board_prepare component=display result=error");
        (void)mybot_sensecap_set_codec_power(false);
        return -1;
    }
    if (board_show_screen(MYBOT_LCD_SCREEN_STARTING) < 0) {
        s_lcd_ops->destroy(s_lcd_context);
        s_lcd_context = NULL;
        (void)mybot_sensecap_set_codec_power(false);
        ESP_LOGE(TAG, "event=board_prepare component=display result=error reason=render");
        return -1;
    }
    if (mybot_sensecap_input_start() < 0) {
        s_lcd_ops->destroy(s_lcd_context);
        s_lcd_context = NULL;
        (void)mybot_sensecap_set_codec_power(false);
        ESP_LOGE(TAG, "event=board_prepare component=input result=error");
        return -1;
    }

    ESP_LOGI(TAG, "event=board_prepare board=%s result=ok", MYBOT_BOARD_NAME);
    return 0;
}

int mybot_board_handle_boot_long_press(void) {
    mybot_board_request_wifi_provisioning();
    return 0;
}

static int board_provision_wifi(void) {
    return mybot_wifi_run_provisioning(board_on_button_provisioning);
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
        .key = mybot_sensecap_input_ops(),
        .audio_capture = mybot_sensecap_audio_capture_ops(),
        .audio_playback = mybot_sensecap_audio_playback_ops(),
        .audio_volume = mybot_sensecap_audio_volume_ops(),
        .announce = mybot_esp32s3_announce_ops(),
        .https = mybot_esp32s3_https_ops(),
        .lcd = mybot_sensecap_lcd_ops(),
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
