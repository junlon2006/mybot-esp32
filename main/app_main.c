/* SPDX-License-Identifier: Apache-2.0 */
#include <mybot/mybot.h>
#include <mybot/mybot_version.h>
#include <agora_rtc_api.h>

#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "mybot_board.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define TAG "mybot_bootstrap"
#define CONTROL_EVENT_WAIT_MS 100

static esp_event_handler_instance_t s_got_ip_handler;
static bool s_sntp_initialized;

static int init_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err == ESP_OK ? 0 : -1;
}

/* AOSL creates socket-backed wakeup pipes before the platform Wi-Fi init callback runs. */
static int init_network_stack(void) {
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif initialization failed: %s", esp_err_to_name(err));
        return -1;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "default event loop initialization failed: %s", esp_err_to_name(err));
        return -1;
    }
    return 0;
}

static void on_time_sync(struct timeval *tv) {
    (void)tv;
    ESP_LOGI(TAG, "system time synchronized via SNTP");
}

static void start_sntp_on_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id,
                                 void *event_data) {
    (void)arg;
    (void)event_base;
    (void)event_id;
    (void)event_data;

    esp_err_t err = esp_netif_sntp_start();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "SNTP time sync started");
    } else {
        ESP_LOGE(TAG, "failed to start SNTP: %s", esp_err_to_name(err));
    }
}

static int init_time_sync(void) {
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.start = false;
    config.wait_for_sync = false;
    config.sync_cb = on_time_sync;

    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SNTP initialization failed: %s", esp_err_to_name(err));
        return -1;
    }

    /* This handler is registered before Wi-Fi so time sync starts before the platform publishes
     * usable connectivity to mybot. */
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, start_sntp_on_got_ip,
                                              NULL, &s_got_ip_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SNTP IP handler registration failed: %s", esp_err_to_name(err));
        esp_netif_sntp_deinit();
        return -1;
    }
    s_sntp_initialized = true;
    return 0;
}

static void deinit_time_sync(void) {
    if (!s_sntp_initialized) {
        return;
    }
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_got_ip_handler);
    esp_netif_sntp_deinit();
    s_sntp_initialized = false;
}

void app_main(void) {
    ESP_LOGI(TAG, "mybot %s, Agora RTSA %s, ESP-IDF %s", mybot_version_string(),
             agora_rtc_get_version(), esp_get_idf_version());

    if (init_nvs() < 0) {
        ESP_LOGE(TAG, "NVS initialization failed");
        return;
    }
    if (init_network_stack() < 0) {
        return;
    }

    if (!esp_psram_is_initialized()) {
        ESP_LOGE(TAG, "PSRAM is required but was not initialized");
        return;
    }
    if (mybot_board_register() < 0) {
        return;
    }
    const mybot_board_t *board = mybot_board_get();

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
    snprintf(config.hw_model, sizeof(config.hw_model), "%s", board->hw_model);

    (void)init_time_sync();
    for (;;) {
        int64_t network_started_us = esp_timer_get_time();
        ESP_LOGI(TAG, "event=control state=waiting_network");
        if (board->ensure_network(config.device_id) < 0) {
            ESP_LOGE(TAG, "event=control state=waiting_network elapsed_ms=%" PRId64 " result=error",
                     (esp_timer_get_time() - network_started_us) / 1000);
            break;
        }
        int64_t mybot_started_us = esp_timer_get_time();
        ESP_LOGI(TAG, "event=control state=starting_mybot network_elapsed_ms=%" PRId64,
                 (mybot_started_us - network_started_us) / 1000);
        if (mybot_start(&config) < 0) {
            ESP_LOGE(TAG, "event=control state=starting_mybot elapsed_ms=%" PRId64 " result=error",
                     (esp_timer_get_time() - mybot_started_us) / 1000);
            break;
        }
        ESP_LOGI(TAG, "event=control state=mybot_running start_elapsed_ms=%" PRId64 " device=%s",
                 (esp_timer_get_time() - mybot_started_us) / 1000, config.device_id);

        bool provisioning_requested = false;
        while (mybot_is_running()) {
            if (mybot_board_wait_wifi_provisioning_request(CONTROL_EVENT_WAIT_MS)) {
                provisioning_requested = true;
                break;
            }
        }
        if (!provisioning_requested) {
            provisioning_requested = mybot_board_wait_wifi_provisioning_request(0);
        }
        int64_t stop_started_us = esp_timer_get_time();
        ESP_LOGI(TAG, "event=control state=stopping_mybot sdk_state=%d running=%d reason=%s",
                 (int)mybot_get_state(), mybot_is_running() ? 1 : 0,
                 provisioning_requested ? "provision_request" : "sdk_stopped");
        mybot_stop();
        if (!provisioning_requested) {
            provisioning_requested = mybot_board_wait_wifi_provisioning_request(0);
        }

        if (!provisioning_requested) {
            ESP_LOGI(TAG,
                     "event=control state=stopped stop_elapsed_ms=%" PRId64 " reason=sdk_stopped",
                     (esp_timer_get_time() - stop_started_us) / 1000);
            break;
        }
        int64_t provision_started_us = esp_timer_get_time();
        ESP_LOGI(TAG, "event=control state=provisioning stop_elapsed_ms=%" PRId64 " source=button",
                 (provision_started_us - stop_started_us) / 1000);
        if (board->provision_wifi() < 0) {
            ESP_LOGE(TAG, "event=control state=provisioning elapsed_ms=%" PRId64 " result=error",
                     (esp_timer_get_time() - provision_started_us) / 1000);
            break;
        }
        (void)mybot_board_wait_wifi_provisioning_request(0);
    }

    ESP_LOGI(TAG, "event=control state=shutting_down");
    board->shutdown_network();
    deinit_time_sync();
}
