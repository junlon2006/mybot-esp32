/* SPDX-License-Identifier: Apache-2.0 */
#include <mybot/platform/mybot_wifi.h>

#include <ssid_manager.h>
#include <wifi_manager.h>

#include "wifi_control.h"

#include "esp_log.h"
#include "esp_mac.h"

#include <chrono>
#include <cstdio>
#include <condition_variable>
#include <mutex>
#include <string>

#define TAG "mybot_wifi"

namespace {

constexpr auto kStationConnectionTimeout = std::chrono::seconds(30);

struct WifiControl {
    std::mutex operation_mutex;
    std::mutex event_mutex;
    std::mutex state_mutex;
    std::condition_variable state_changed;
    bool initialized = false;
    bool stopping = false;
    bool network_connected = false;
    bool config_mode = false;
    bool sdk_active = false;
    bool sdk_connected = false;
    mybot_wifi_event_handler_t emit = nullptr;
    void *user_data = nullptr;
};

WifiControl control;

void emit_sdk_transition_locked(mybot_wifi_event_t event) {
    mybot_wifi_event_handler_t emit = nullptr;
    void *user_data = nullptr;
    {
        std::lock_guard<std::mutex> lock(control.state_mutex);
        if (!control.sdk_active || !control.emit) {
            return;
        }
        if (event == MYBOT_WIFI_EVENT_STA_CONNECTED) {
            if (control.sdk_connected) {
                return;
            }
            control.sdk_connected = true;
        } else if (event == MYBOT_WIFI_EVENT_STA_DISCONNECTED) {
            if (!control.sdk_connected) {
                return;
            }
            control.sdk_connected = false;
        }
        emit = control.emit;
        user_data = control.user_data;
    }

    emit(event, user_data);
}

void handle_wifi_event(WifiEvent event, const std::string &data) {
    (void)data;
    std::lock_guard<std::mutex> event_lock(control.event_mutex);

    {
        std::lock_guard<std::mutex> lock(control.state_mutex);
        if (!control.initialized) {
            return;
        }
        switch (event) {
        case WifiEvent::Connected:
            control.network_connected = true;
            break;
        case WifiEvent::Disconnected:
            control.network_connected = false;
            break;
        case WifiEvent::ConfigModeEnter:
            control.config_mode = true;
            control.network_connected = false;
            break;
        case WifiEvent::ConfigModeExit:
            control.config_mode = false;
            break;
        default:
            break;
        }
    }
    control.state_changed.notify_all();

    if (event == WifiEvent::Connected) {
        emit_sdk_transition_locked(MYBOT_WIFI_EVENT_STA_CONNECTED);
    } else if (event == WifiEvent::Disconnected) {
        emit_sdk_transition_locked(MYBOT_WIFI_EVENT_STA_DISCONNECTED);
    }
}

bool wait_for_station(std::chrono::seconds timeout) {
    std::unique_lock<std::mutex> lock(control.state_mutex);
    bool ready = control.state_changed.wait_for(
        lock, timeout, [] { return control.network_connected || !control.initialized; });
    return ready && control.initialized && control.network_connected;
}

int run_provisioning_locked(mybot_wifi_provisioning_handler_t on_provisioning) {
    auto &manager = WifiManager::GetInstance();
    bool provisioning_announced = false;

    for (;;) {
        {
            std::lock_guard<std::mutex> lock(control.state_mutex);
            if (!control.initialized) {
                return -1;
            }
            control.network_connected = false;
        }

        ESP_LOGI(TAG, "event=provision_ap state=starting");
        manager.StartConfigAp();
        if (!manager.IsConfigMode()) {
            ESP_LOGE(TAG, "event=provision_ap state=starting result=error");
            return -1;
        }
        if (!provisioning_announced && on_provisioning) {
            provisioning_announced = true;
            on_provisioning();
        }

        {
            std::unique_lock<std::mutex> lock(control.state_mutex);
            control.state_changed.wait(lock,
                                       [] { return !control.config_mode || !control.initialized; });
            if (!control.initialized) {
                return -1;
            }
        }

        ESP_LOGI(TAG, "event=station_connect source=provisioning state=waiting_ip");
        manager.StartStation();
        if (wait_for_station(kStationConnectionTimeout)) {
            return 0;
        }
        ESP_LOGW(TAG, "event=station_connect source=provisioning result=timeout "
                      "action=restart_provisioning");
    }
}

int wifi_init(void **out_ctx, const char *device_id, mybot_wifi_event_handler_t emit,
              void *user_data) {
    ESP_LOGI(TAG, "event=sdk_adapter adapter=wifi action=attach");
    if (!out_ctx || !device_id || !device_id[0] || !emit) {
        return -1;
    }
    *out_ctx = nullptr;

    std::lock_guard<std::mutex> event_lock(control.event_mutex);
    bool connected = false;
    {
        std::lock_guard<std::mutex> lock(control.state_mutex);
        if (!control.initialized || control.stopping || control.sdk_active) {
            ESP_LOGE(TAG, "event=sdk_network_attach result=error reason=network_unavailable");
            return -1;
        }
        control.sdk_active = true;
        control.sdk_connected = false;
        control.emit = emit;
        control.user_data = user_data;
        connected = control.network_connected;
    }

    *out_ctx = &control;
    if (connected) {
        emit_sdk_transition_locked(MYBOT_WIFI_EVENT_STA_CONNECTED);
    }
    return 0;
}

void wifi_destroy(void *opaque) {
    if (opaque != &control) {
        return;
    }

    ESP_LOGI(TAG, "event=sdk_adapter adapter=wifi action=detach");
    std::lock_guard<std::mutex> event_lock(control.event_mutex);
    std::lock_guard<std::mutex> lock(control.state_mutex);
    control.sdk_active = false;
    control.sdk_connected = false;
    control.emit = nullptr;
    control.user_data = nullptr;
}

const mybot_wifi_ops_t wifi_ops = {
    .init = wifi_init,
    .destroy = wifi_destroy,
};

} // namespace

extern "C" const mybot_wifi_ops_t *mybot_esp32s3_wifi_ops(void) {
    return &wifi_ops;
}

extern "C" int mybot_wifi_ensure_network(const char *device_id,
                                         mybot_wifi_provisioning_handler_t on_provisioning) {
    if (!device_id || !device_id[0]) {
        return -1;
    }

    std::lock_guard<std::mutex> operation_lock(control.operation_mutex);

    auto &manager = WifiManager::GetInstance();
    bool initialized = false;
    {
        std::lock_guard<std::mutex> lock(control.state_mutex);
        if (control.stopping) {
            return -1;
        }
        initialized = control.initialized;
        if (initialized && control.network_connected) {
            return 0;
        }
    }

    if (!initialized) {
        uint8_t mac[6];
        if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
            ESP_LOGE(TAG, "event=network_prepare result=error reason=mac_read");
            return -1;
        }
        char provisioning_ssid[sizeof("mybot-ffff")];
        snprintf(provisioning_ssid, sizeof(provisioning_ssid), "mybot-%02x%02x", mac[0], mac[1]);

        WifiManagerConfig config;
        config.ap_ssid = provisioning_ssid;
        config.ssid_prefix = "mybot";
        config.language = CONFIG_MYBOT_LANGUAGE_TAG;
        config.station_hostname = device_id;

        if (!manager.Initialize(config)) {
            ESP_LOGE(TAG, "event=network_prepare result=error reason=manager_initialize");
            return -1;
        }

        bool connected = manager.IsConnected();
        bool config_mode = manager.IsConfigMode();
        {
            std::lock_guard<std::mutex> event_lock(control.event_mutex);
            std::lock_guard<std::mutex> lock(control.state_mutex);
            if (control.stopping) {
                return -1;
            }
            control.initialized = true;
            control.network_connected = connected;
            control.config_mode = config_mode;
        }
        manager.SetEventCallback(handle_wifi_event);

        if (connected) {
            return 0;
        }
    } else if (manager.IsConnected()) {
        std::lock_guard<std::mutex> event_lock(control.event_mutex);
        std::lock_guard<std::mutex> lock(control.state_mutex);
        if (!control.initialized || control.stopping) {
            return -1;
        }
        control.network_connected = true;
        return 0;
    }

    if (SsidManager::GetInstance().GetSsidList().empty()) {
        ESP_LOGI(TAG, "event=network_route action=provision reason=no_saved_credentials");
        return run_provisioning_locked(on_provisioning);
    }

    ESP_LOGI(TAG, "event=station_connect source=saved_credentials state=waiting_ip");
    manager.StartStation();
    if (wait_for_station(kStationConnectionTimeout)) {
        return 0;
    }

    ESP_LOGW(TAG, "event=station_connect source=saved_credentials result=timeout action=provision");
    return run_provisioning_locked(on_provisioning);
}

extern "C" int mybot_wifi_run_provisioning(mybot_wifi_provisioning_handler_t on_provisioning) {
    std::lock_guard<std::mutex> operation_lock(control.operation_mutex);
    return run_provisioning_locked(on_provisioning);
}

extern "C" void mybot_wifi_shutdown_network(void) {
    auto &manager = WifiManager::GetInstance();
    {
        std::lock_guard<std::mutex> event_lock(control.event_mutex);
        std::lock_guard<std::mutex> lock(control.state_mutex);
        control.stopping = true;
        control.initialized = false;
        control.network_connected = false;
        control.config_mode = false;
        control.sdk_active = false;
        control.sdk_connected = false;
        control.emit = nullptr;
        control.user_data = nullptr;
    }
    control.state_changed.notify_all();

    std::lock_guard<std::mutex> operation_lock(control.operation_mutex);
    manager.SetEventCallback({});
    manager.StopStation();
    manager.StopConfigAp();
}
