/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_esp32s3_platform.h"

#include "esp_log.h"

#define TAG "mybot_platform"

int mybot_esp32s3_audio_register(void);
int mybot_esp32s3_buttons_register(void);
int mybot_esp32s3_https_register(void);
int mybot_esp32s3_kv_store_register(void);
int mybot_esp32s3_lcd_register(void);
int mybot_esp32s3_wifi_register(void);

int mybot_esp32s3_platform_register(void) {
    if (mybot_esp32s3_https_register() < 0 || mybot_esp32s3_kv_store_register() < 0 ||
        mybot_esp32s3_wifi_register() < 0 || mybot_esp32s3_audio_register() < 0 ||
        mybot_esp32s3_buttons_register() < 0 || mybot_esp32s3_lcd_register() < 0) {
        ESP_LOGE(TAG, "platform adapter registration failed");
        return -1;
    }
    ESP_LOGI(TAG, "zhengchen platform adapters registered");
    return 0;
}
