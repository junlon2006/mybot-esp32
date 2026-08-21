/* SPDX-License-Identifier: Apache-2.0 */
#include <mybot/mybot.h>
#include <mybot/mybot_version.h>
#include <agora_rtc_api.h>

#include "board_config.h"
#include "esp_app_desc.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "mybot_esp32s3_platform.h"

#include <stdio.h>
#include <string.h>

#define TAG "mybot_bootstrap"

static int init_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err == ESP_OK ? 0 : -1;
}

void app_main(void) {
    ESP_LOGI(TAG, "mybot %s, Agora RTSA %s, ESP-IDF %s", mybot_version_string(),
             agora_rtc_get_version(), esp_get_idf_version());

    if (init_nvs() < 0) {
        ESP_LOGE(TAG, "NVS initialization failed");
        return;
    }

    if (!esp_psram_is_initialized()) {
        ESP_LOGE(TAG, "PSRAM is required but was not initialized");
        return;
    }
    if (mybot_esp32s3_platform_register() < 0) {
        return;
    }

    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        ESP_LOGE(TAG, "failed to read device MAC");
        return;
    }

    mybot_config_t config = {0};
    const esp_app_desc_t *app_description = esp_app_get_description();
    snprintf(config.device_id, sizeof(config.device_id), "esp32s3-%02x%02x%02x%02x%02x%02x", mac[0],
             mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(config.server_base, sizeof(config.server_base), "%s", CONFIG_MYBOT_SERVER_BASE);
    snprintf(config.firmware_ver, sizeof(config.firmware_ver), "%s", app_description->version);
    snprintf(config.hw_model, sizeof(config.hw_model), "%s", MYBOT_BOARD_NAME);

    if (mybot_start(&config) < 0) {
        ESP_LOGE(TAG, "mybot startup failed");
        return;
    }
    ESP_LOGI(TAG, "mybot startup scheduled for device %s", config.device_id);

    while (mybot_is_running()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    mybot_stop();
    ESP_LOGI(TAG, "mybot stopped");
}
