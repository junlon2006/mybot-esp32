/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_board.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TAG "mybot_platform"

static StaticSemaphore_t s_wifi_provisioning_event_storage;
static SemaphoreHandle_t s_wifi_provisioning_event;

int mybot_board_register(void) {
    if (!s_wifi_provisioning_event) {
        s_wifi_provisioning_event =
            xSemaphoreCreateBinaryStatic(&s_wifi_provisioning_event_storage);
    }
    const mybot_board_t *board = mybot_board_get();
    if (!s_wifi_provisioning_event || !board || !board->id || !board->hw_model ||
        !board->register_platform || !board->ensure_network || !board->provision_wifi ||
        !board->shutdown_network) {
        ESP_LOGE(TAG, "event=platform_register result=error reason=board_descriptor");
        return -1;
    }
    if (board->prepare && board->prepare() < 0) {
        ESP_LOGE(TAG, "event=platform_register board=%s result=error reason=board_prepare",
                 board->id);
        return -1;
    }
    if (board->register_platform() < 0) {
        ESP_LOGE(TAG, "event=platform_register board=%s result=error reason=platform_descriptor",
                 board->id);
        return -1;
    }
    ESP_LOGI(TAG, "event=platform_register board=%s result=ok", board->id);
    return 0;
}

void mybot_board_request_wifi_provisioning(void) {
    if (s_wifi_provisioning_event) {
        (void)xSemaphoreGive(s_wifi_provisioning_event);
    }
}

bool mybot_board_wait_wifi_provisioning_request(uint32_t timeout_ms) {
    if (!s_wifi_provisioning_event) {
        return false;
    }
    return xSemaphoreTake(s_wifi_provisioning_event, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}
