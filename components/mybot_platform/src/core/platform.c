/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_board.h"

#include "esp_log.h"

#define TAG "mybot_platform"

int mybot_board_register(void) {
    const mybot_board_t *board = mybot_board_get();
    if (!board || !board->id || !board->hw_model || !board->register_platform) {
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
