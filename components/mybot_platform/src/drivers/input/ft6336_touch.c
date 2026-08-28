/* SPDX-License-Identifier: Apache-2.0 */
#include "board_config.h"

#include <mybot/mybot.h>
#include <mybot/platform/mybot_key.h>

#include "board_actions.h"
#include "cores3_hardware.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdint.h>

#define TAG "cores3_touch"
#define I2C_TIMEOUT_MS 100
#define TOUCH_TASK_STACK_SIZE 3072
#define TOUCH_TASK_PRIORITY 4

typedef struct {
    portMUX_TYPE lock;
    i2c_master_dev_handle_t device;
    TaskHandle_t task;
    bool started;
    bool active;
    unsigned int callbacks_in_flight;
    mybot_key_event_handler_t emit;
    void *user_data;
} touch_context_t;

static touch_context_t s_context = {
    .lock = portMUX_INITIALIZER_UNLOCKED,
};

static esp_err_t read_registers(uint8_t reg, uint8_t *data, size_t size) {
    return i2c_master_transmit_receive(s_context.device, &reg, sizeof(reg), data, size,
                                       I2C_TIMEOUT_MS);
}

static void emit_event(mybot_key_event_t event) {
    mybot_key_event_handler_t emit = NULL;
    void *user_data = NULL;

    portENTER_CRITICAL(&s_context.lock);
    if (s_context.active && s_context.emit) {
        ++s_context.callbacks_in_flight;
        emit = s_context.emit;
        user_data = s_context.user_data;
    }
    portEXIT_CRITICAL(&s_context.lock);
    if (!emit) {
        return;
    }

    emit(event, user_data);

    portENTER_CRITICAL(&s_context.lock);
    --s_context.callbacks_in_flight;
    portEXIT_CRITICAL(&s_context.lock);
}

static void handle_short_touch(void) {
    const mybot_state_t state = mybot_get_state();
    if (state == MYBOT_STATE_READY) {
        emit_event(MYBOT_KEY_EVENT_CONVERSATION_START);
    } else if (state == MYBOT_STATE_IN_CONVERSATION) {
        emit_event(MYBOT_KEY_EVENT_CONVERSATION_STOP);
    }
}

static void touch_task(void *argument) {
    (void)argument;
    bool was_touched = false;
    bool long_press_sent = false;
    bool read_failed = false;
    int64_t touch_started_ms = 0;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        uint8_t touch_data[6];
        const esp_err_t result = read_registers(0x02, touch_data, sizeof(touch_data));
        if (result != ESP_OK) {
            if (!read_failed) {
                ESP_LOGW(TAG, "event=touch action=read result=error code=%s",
                         esp_err_to_name(result));
                read_failed = true;
            }
            was_touched = false;
            long_press_sent = false;
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(MYBOT_TOUCH_POLL_INTERVAL_MS));
            continue;
        }
        if (read_failed) {
            ESP_LOGI(TAG, "event=touch action=read result=recovered");
            read_failed = false;
        }

        const bool touched = (touch_data[0] & 0x0fU) != 0;
        const int64_t now_ms = esp_timer_get_time() / 1000;
        if (touched && !was_touched) {
            was_touched = true;
            long_press_sent = false;
            touch_started_ms = now_ms;
        } else if (touched && !long_press_sent &&
                   now_ms - touch_started_ms >= MYBOT_TOUCH_LONG_PRESS_MS) {
            long_press_sent = true;
            ESP_LOGI(TAG, "event=provision_request source=touch action=long_press");
            if (mybot_board_handle_boot_long_press() < 0) {
                ESP_LOGE(TAG, "event=provision_request source=touch result=error");
            }
        } else if (!touched && was_touched) {
            const int64_t duration_ms = now_ms - touch_started_ms;
            was_touched = false;
            if (!long_press_sent && duration_ms >= MYBOT_TOUCH_SHORT_PRESS_MIN_MS) {
                handle_short_touch();
            }
            long_press_sent = false;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(MYBOT_TOUCH_POLL_INTERVAL_MS));
    }
}

int mybot_cores3_touch_start(void) {
    if (s_context.started) {
        return 0;
    }
    i2c_master_bus_handle_t bus = mybot_cores3_i2c_bus_handle();
    if (!bus) {
        return -1;
    }

    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MYBOT_CORES3_FT6336_ADDRESS,
        .scl_speed_hz = MYBOT_CORES3_I2C_SPEED_HZ,
    };
    if (i2c_master_bus_add_device(bus, &config, &s_context.device) != ESP_OK) {
        ESP_LOGE(TAG, "event=touch action=initialize result=error reason=i2c_device");
        return -1;
    }

    uint8_t chip_id = 0;
    if (read_registers(0xa3, &chip_id, sizeof(chip_id)) != ESP_OK) {
        i2c_master_bus_rm_device(s_context.device);
        s_context.device = NULL;
        ESP_LOGE(TAG, "event=touch action=initialize result=error reason=chip_id");
        return -1;
    }
    if (xTaskCreate(touch_task, "cores3_touch", TOUCH_TASK_STACK_SIZE, NULL, TOUCH_TASK_PRIORITY,
                    &s_context.task) != pdPASS) {
        i2c_master_bus_rm_device(s_context.device);
        s_context.device = NULL;
        ESP_LOGE(TAG, "event=touch action=initialize result=error reason=task");
        return -1;
    }
    s_context.started = true;
    ESP_LOGI(TAG, "event=touch action=initialize result=ok chip_id=0x%02x interval_ms=%d", chip_id,
             MYBOT_TOUCH_POLL_INTERVAL_MS);
    return 0;
}

static int touch_init(void **out_ctx, mybot_key_event_handler_t emit, void *user_data) {
    if (!out_ctx || !emit) {
        return -1;
    }
    *out_ctx = NULL;

    portENTER_CRITICAL(&s_context.lock);
    if (!s_context.started || s_context.active) {
        portEXIT_CRITICAL(&s_context.lock);
        return -1;
    }
    s_context.emit = emit;
    s_context.user_data = user_data;
    s_context.active = true;
    portEXIT_CRITICAL(&s_context.lock);

    *out_ctx = &s_context;
    ESP_LOGI(TAG, "event=sdk_adapter adapter=keys action=attach source=touch");
    return 0;
}

static void touch_destroy(void *opaque) {
    if (opaque != &s_context) {
        return;
    }

    /* Polling is Board-owned so long press can request provisioning while the SDK is stopped.
     * Detaching this adapter disables SDK key emission and waits for callbacks already in flight.
     */
    portENTER_CRITICAL(&s_context.lock);
    s_context.active = false;
    unsigned int in_flight = s_context.callbacks_in_flight;
    portEXIT_CRITICAL(&s_context.lock);
    ESP_LOGI(TAG, "event=sdk_adapter adapter=keys action=detach callbacks_in_flight=%u", in_flight);

    for (;;) {
        portENTER_CRITICAL(&s_context.lock);
        in_flight = s_context.callbacks_in_flight;
        portEXIT_CRITICAL(&s_context.lock);
        if (in_flight == 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    portENTER_CRITICAL(&s_context.lock);
    s_context.emit = NULL;
    s_context.user_data = NULL;
    portEXIT_CRITICAL(&s_context.lock);
}

static const mybot_key_ops_t s_ops = {
    .init = touch_init,
    .destroy = touch_destroy,
};

const mybot_key_ops_t *mybot_cores3_touch_ops(void) {
    return &s_ops;
}
