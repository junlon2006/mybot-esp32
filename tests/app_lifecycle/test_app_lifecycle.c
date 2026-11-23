/* SPDX-License-Identifier: Apache-2.0 */
#include "host_idf.h"

#include <agora_rtc_api.h>
#include <mybot/mybot.h>
#include "mybot_platform/board.h"
#include "mybot_platform/display_status.h"

#include <stdio.h>

void app_main(void);

static struct {
    bool start_fails;
    bool network_up;
    bool time_sync_up;
    bool media_active;
    bool worker_active;
    bool running;
    bool button_pressed;
    bool retry_pending;
    unsigned int starts;
    unsigned int stops;
    unsigned int delays;
    unsigned int provisions;
    unsigned int shutdowns;
    unsigned int stop_timeouts;
    mybot_state_t state;
    const char *display_state;
    esp_event_handler_instance_t got_ip;
} app;

static void assert_sdk_released(void) {
    assert(app.state == MYBOT_STATE_STOPPED);
    assert(strcmp(app.display_state, "stopped") == 0);
    assert(!app.running && !app.media_active && !app.worker_active && !app.retry_pending);
}

void host_log(const char *tag, const char *format, ...) {
    (void)tag;
    (void)format;
}

int64_t esp_timer_get_time(void) {
    static int64_t now;
    return now += 1000;
}

const char *mybot_version_string(void) {
    return "host SDK";
}

const char *agora_rtc_get_version(void) {
    return "host RTSA";
}

int mybot_display_set_runtime_status(const char *state, bool in_conversation) {
    assert(in_conversation == (app.state == MYBOT_STATE_IN_CONVERSATION));
    app.display_state = state;
    return 0;
}

esp_err_t esp_event_handler_instance_register(esp_event_base_t base, int32_t id,
                                              esp_event_handler_instance_t handler, void *arg,
                                              esp_event_handler_instance_t *instance) {
    assert(app.time_sync_up && !app.got_ip);
    assert(strcmp(base, IP_EVENT) == 0 && id == IP_EVENT_STA_GOT_IP && !arg);
    *instance = handler;
    app.got_ip = handler;
    return ESP_OK;
}

esp_err_t esp_event_handler_instance_unregister(esp_event_base_t base, int32_t id,
                                                esp_event_handler_instance_t instance) {
    assert(app.time_sync_up && app.got_ip == instance);
    assert(strcmp(base, IP_EVENT) == 0 && id == IP_EVENT_STA_GOT_IP);
    assert_sdk_released();
    app.got_ip = NULL;
    return ESP_OK;
}

esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *config) {
    assert(!app.time_sync_up && !config->start && !config->wait_for_sync && config->sync_cb);
    app.time_sync_up = true;
    return ESP_OK;
}

esp_err_t esp_netif_sntp_start(void) {
    assert(app.time_sync_up && app.network_up);
    return ESP_OK;
}

void esp_netif_sntp_deinit(void) {
    assert(app.time_sync_up && !app.got_ip && !app.network_up);
    app.time_sync_up = false;
}

static int ensure_network(const char *device_id) {
    assert_sdk_released();
    assert(device_id[0] && app.got_ip);
    app.network_up = true;
    app.got_ip(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, NULL);
    return 0;
}

static int provision_wifi(void) {
    assert_sdk_released();
    assert(app.network_up && app.time_sync_up);
    assert(++app.provisions == app.starts);
    return 0;
}

static void shutdown_network(void) {
    assert_sdk_released();
    assert(app.network_up && app.time_sync_up);
    ++app.shutdowns;
    app.network_up = false;
}

int mybot_board_register(void) {
    return 0;
}

const mybot_board_t *mybot_board_get(void) {
    static const mybot_board_t board = {
        .hw_model = "host",
        .ensure_network = ensure_network,
        .provision_wifi = provision_wifi,
        .shutdown_network = shutdown_network,
    };
    return &board;
}

bool mybot_board_wait_wifi_provisioning_request(uint32_t timeout_ms) {
    if (timeout_ms > 0) {
        assert(app.running && app.state == MYBOT_STATE_READY);
        assert(app.starts == 1 && !app.start_fails);
        app.button_pressed = true;
    }
    const bool requested = app.button_pressed;
    app.button_pressed = false;
    return requested;
}

int mybot_start(const mybot_config_t *config) {
    assert_sdk_released();
    assert(app.network_up && app.time_sync_up && config->device_id[0]);
    assert(app.starts == app.provisions);
    ++app.starts;
    app.worker_active = true;
    app.stop_timeouts = app.starts < 3 ? app.starts : 0;
    if (app.start_fails) {
        /* Failed startup already released media but still owns a worker. */
        app.state = MYBOT_STATE_STOPPING;
        return -1;
    }
    app.media_active = true;
    app.state = MYBOT_STATE_READY;
    app.running = app.starts == 1;
    return 0;
}

bool mybot_is_running(void) {
    return app.running;
}

mybot_state_t mybot_get_state(void) {
    return app.state;
}

void mybot_stop(void) {
    assert(app.network_up && app.time_sync_up);
    assert(app.worker_active && !app.retry_pending);
    ++app.stops;
    app.running = false;
    app.media_active = false;
    if (app.stop_timeouts > 0) {
        --app.stop_timeouts;
        app.state = MYBOT_STATE_STOPPING;
        app.retry_pending = true;
        return;
    }
    app.worker_active = false;
    app.state = MYBOT_STATE_STOPPED;
}

void vTaskDelay(TickType_t ticks) {
    assert(ticks > 0 && app.retry_pending);
    assert(app.state == MYBOT_STATE_STOPPING && strcmp(app.display_state, "stopping") == 0);
    assert(app.network_up && app.time_sync_up && app.got_ip);
    assert(app.worker_active && !app.media_active && !app.running);
    ++app.delays;
    app.retry_pending = false;
    if (app.starts == 2) {
        /* A late button event must wait for the worker to exit. */
        app.button_pressed = true;
    }
}

int main(void) {
    app.start_fails = true;
    app_main();
    assert_sdk_released();
    assert(!app.network_up && !app.time_sync_up);
    assert(app.starts == 1 && app.stops == 2 && app.delays == 1);
    assert(app.provisions == 0 && app.shutdowns == 1);

    memset(&app, 0, sizeof(app));
    app_main();
    assert_sdk_released();
    assert(!app.network_up && !app.time_sync_up);
    assert(app.starts == 3 && app.stops == 6 && app.delays == 3);
    assert(app.provisions == 2 && app.shutdowns == 1);
    puts("app lifecycle tests passed");
    return 0;
}
