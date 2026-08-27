/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_board.h"

#include <api/aosl_atomic.h>

#include "esp_log.h"

#define TAG "mybot_platform"

static aosl_atomic_t s_wifi_provisioning_requested;

int mybot_board_register(void) {
    const mybot_board_t *board = mybot_board_get();
    if (!board || !board->id || !board->hw_model || !board->register_platform ||
        !board->ensure_network || !board->provision_wifi || !board->shutdown_network) {
        ESP_LOGE(TAG, "invalid board descriptor");
        return -1;
    }
    if (board->prepare && board->prepare() < 0) {
        ESP_LOGE(TAG, "board preparation failed for %s", board->id);
        return -1;
    }
    if (board->register_platform() < 0) {
        ESP_LOGE(TAG, "platform descriptor registration failed");
        return -1;
    }
    return 0;
}

void mybot_board_request_wifi_provisioning(void) {
    aosl_atomic_set(&s_wifi_provisioning_requested, true);
}

bool mybot_board_take_wifi_provisioning_request(void) {
    return aosl_atomic_xchg(&s_wifi_provisioning_requested, false) != 0;
}
