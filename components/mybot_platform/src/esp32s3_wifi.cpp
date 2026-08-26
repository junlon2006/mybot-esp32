/* SPDX-License-Identifier: Apache-2.0 */
#include "board_config.h"

#include <mybot/platform/mybot_wifi.h>

#include <ssid_manager.h>
#include <wifi_manager.h>

#include "esp_log.h"
#include "esp_mac.h"

#include <cstdio>
#include <condition_variable>
#include <mutex>
#include <string>

#define TAG "mybot_wifi"

namespace {

struct WifiBinding {
    std::mutex mutex;
    std::condition_variable drained;
    bool active = false;
    bool connected = false;
    unsigned int callbacks_in_flight = 0;
    mybot_wifi_event_handler_t emit = nullptr;
    void *user_data = nullptr;
};

WifiBinding binding;

bool begin_callback() {
    std::lock_guard<std::mutex> lock(binding.mutex);
    if (!binding.active) {
        return false;
    }
    ++binding.callbacks_in_flight;
    return true;
}

void end_callback() {
    std::lock_guard<std::mutex> lock(binding.mutex);
    --binding.callbacks_in_flight;
    if (binding.callbacks_in_flight == 0) {
        binding.drained.notify_all();
    }
}

void emit_transition(mybot_wifi_event_t event) {
    mybot_wifi_event_handler_t emit = nullptr;
    void *user_data = nullptr;
    {
        std::lock_guard<std::mutex> lock(binding.mutex);
        if (!binding.active || !binding.emit) {
            return;
        }
        if (event == MYBOT_WIFI_EVENT_STA_CONNECTED) {
            if (binding.connected) {
                return;
            }
            binding.connected = true;
        } else if (event == MYBOT_WIFI_EVENT_STA_DISCONNECTED) {
            if (!binding.connected) {
                return;
            }
            binding.connected = false;
        }
        emit = binding.emit;
        user_data = binding.user_data;
    }

    emit(event, user_data);
}

void handle_wifi_event(WifiEvent event, const std::string &data) {
    if (!begin_callback()) {
        return;
    }
    (void)data;
    switch (event) {
    case WifiEvent::Connected:
        emit_transition(MYBOT_WIFI_EVENT_STA_CONNECTED);
        break;
    case WifiEvent::Disconnected:
        emit_transition(MYBOT_WIFI_EVENT_STA_DISCONNECTED);
        break;
    case WifiEvent::ConfigModeExit:
        WifiManager::GetInstance().StartStation();
        break;
    default:
        break;
    }
    end_callback();
}

int wifi_init(void **out_ctx, const char *device_id, mybot_wifi_event_handler_t emit,
              void *user_data) {
    if (!out_ctx || !device_id || !device_id[0] || !emit) {
        return -1;
    }
    *out_ctx = nullptr;

    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        ESP_LOGE(TAG, "failed to read station MAC for provisioning SSID");
        return -1;
    }
    char provisioning_ssid[sizeof("mybot-ffff")];
    snprintf(provisioning_ssid, sizeof(provisioning_ssid), "mybot-%02x%02x", mac[0], mac[1]);

    {
        std::lock_guard<std::mutex> lock(binding.mutex);
        if (binding.active) {
            return -1;
        }
        binding.active = true;
        binding.connected = false;
        binding.emit = emit;
        binding.user_data = user_data;
    }

    WifiManagerConfig config;
    config.ap_ssid = provisioning_ssid;
    config.ssid_prefix = "mybot";
    config.language = "zh-CN";
    config.station_hostname = device_id;

    auto &manager = WifiManager::GetInstance();
    if (!manager.Initialize(config)) {
        std::lock_guard<std::mutex> lock(binding.mutex);
        binding.active = false;
        binding.emit = nullptr;
        binding.user_data = nullptr;
        return -1;
    }
    manager.SetEventCallback(handle_wifi_event);

    if (SsidManager::GetInstance().GetSsidList().empty()) {
        ESP_LOGI(TAG, "no saved Wi-Fi credentials; starting configuration AP");
        manager.StartConfigAp();
    } else {
        manager.StartStation();
    }

    *out_ctx = &binding;
    return 0;
}

void wifi_destroy(void *opaque) {
    if (opaque != &binding) {
        return;
    }

    auto &manager = WifiManager::GetInstance();
    {
        std::lock_guard<std::mutex> lock(binding.mutex);
        binding.active = false;
    }
    manager.SetEventCallback({});

    std::unique_lock<std::mutex> lock(binding.mutex);
    binding.drained.wait(lock, [] { return binding.callbacks_in_flight == 0; });
    binding.connected = false;
    binding.emit = nullptr;
    binding.user_data = nullptr;
    lock.unlock();

    manager.StopStation();
    manager.StopConfigAp();
}

const mybot_wifi_ops_t wifi_ops = {
    .init = wifi_init,
    .destroy = wifi_destroy,
};

} // namespace

extern "C" const mybot_wifi_ops_t *mybot_esp32s3_wifi_ops(void) {
    return &wifi_ops;
}
