/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Project Contributors */
#include "mybot_platform/board.h"

#include <mybot/platform/mybot_platform.h>

#include "announcement/embedded_ogg_prompt.h"
#include "atom_echos3r_hardware.h"
#include "audio/es8311_board.h"
#include "board_config.h"
#include "esp_log.h"
#include "network/wifi_control.h"
#include "platform/board_actions.h"

#include <stdbool.h>

#define TAG "mybot_platform"

const mybot_audio_capture_ops_t *mybot_es8311_audio_capture_ops(void);
const mybot_audio_playback_ops_t *mybot_es8311_audio_playback_ops(void);
const mybot_audio_volume_ops_t *mybot_es8311_audio_volume_ops(void);
const mybot_announce_ops_t *mybot_esp32s3_announce_ops(void);
const mybot_https_ops_t *mybot_esp32s3_https_ops(void);
const mybot_key_ops_t *mybot_atom_echos3r_input_ops(void);
const mybot_kv_store_ops_t *mybot_esp32s3_kv_store_ops(void);
const mybot_wifi_ops_t *mybot_esp32s3_wifi_ops(void);
int mybot_atom_echos3r_input_start(void);

static bool s_automatic_provisioning_started;

static void board_on_provisioning(const char *trigger) {
    ESP_LOGI(TAG, "event=provisioning action=announce trigger=%s", trigger);
    (void)mybot_embedded_ogg_play_wifi_provisioning(mybot_es8311_audio_playback_ops(),
                                                    mybot_es8311_audio_volume_ops(), trigger);
}

static void board_on_automatic_provisioning(void) {
    s_automatic_provisioning_started = true;
    board_on_provisioning("automatic");
}

static void board_on_button_provisioning(void) {
    board_on_provisioning("button");
}

static int board_prepare(void) {
    if (mybot_atom_echos3r_hardware_init() < 0) {
        ESP_LOGE(TAG, "event=board_prepare component=hardware result=error");
        return -1;
    }
    if (mybot_atom_echos3r_input_start() < 0) {
        if (mybot_atom_echos3r_hardware_deinit() < 0) {
            ESP_LOGE(TAG, "event=board_prepare action=rollback result=error");
        }
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
        .key = mybot_atom_echos3r_input_ops(),
        .audio_capture = mybot_es8311_audio_capture_ops(),
        .audio_playback = mybot_es8311_audio_playback_ops(),
        .audio_volume = mybot_es8311_audio_volume_ops(),
        .announce = mybot_esp32s3_announce_ops(),
        .https = mybot_esp32s3_https_ops(),
        .lcd = NULL,
    };
    return mybot_platform_register(&descriptor);
}

void *mybot_es8311_board_i2c_bus_handle(void) {
    return mybot_atom_echos3r_i2c_bus_handle();
}

int mybot_es8311_board_set_speaker_power(bool enabled) {
    return mybot_atom_echos3r_set_speaker_power(enabled);
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
