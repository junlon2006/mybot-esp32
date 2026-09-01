/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#include "board_config.h"

#include <mybot/mybot.h>
#include <mybot/platform/mybot_key.h>

#include "board_actions.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_knob.h"
#include "sensecap_hardware.h"

#include <stdbool.h>

#define TAG "sensecap_input"
#define BUTTON_TASK_STACK_SIZE 3072
#define BUTTON_TASK_PRIORITY 4
#define READ_FAILURE_LOG_INTERVAL 100
#define STARTUP_RELEASE_READ_RETRIES 10
#define STARTUP_RELEASE_TIMEOUT_MS 30000

typedef struct {
    portMUX_TYPE lock;
    bool active;
    unsigned int callbacks_in_flight;
    mybot_key_event_handler_t emit;
    void *user_data;
    knob_handle_t knob;
    TaskHandle_t button_task;
} input_context_t;

static input_context_t s_context = {
    .lock = portMUX_INITIALIZER_UNLOCKED,
};

static bool adapter_is_active(void) {
    portENTER_CRITICAL(&s_context.lock);
    bool active = s_context.active;
    portEXIT_CRITICAL(&s_context.lock);
    return active;
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

static void handle_button_short_press(void) {
    if (!adapter_is_active()) {
        ESP_LOGI(
            TAG,
            "event=button source=knob action=short_press result=ignored reason=adapter_detached");
        return;
    }

    mybot_state_t state = mybot_get_state();
    if (state == MYBOT_STATE_READY) {
        ESP_LOGI(TAG, "event=button source=knob action=conversation_start");
        emit_event(MYBOT_KEY_EVENT_CONVERSATION_START);
    } else if (state == MYBOT_STATE_IN_CONVERSATION) {
        ESP_LOGI(TAG, "event=button source=knob action=conversation_stop");
        emit_event(MYBOT_KEY_EVENT_CONVERSATION_STOP);
    } else {
        ESP_LOGI(TAG, "event=button source=knob action=short_press result=ignored state=%d",
                 (int)state);
    }
}

static void handle_button_long_press(void) {
    ESP_LOGI(TAG, "event=button source=knob action=wifi_provisioning_request");
    if (mybot_board_handle_boot_long_press() < 0) {
        ESP_LOGE(TAG, "event=provision_request source=knob result=error");
    }
}

static void knob_left(void *knob, void *user_data) {
    (void)knob;
    (void)user_data;
    if (adapter_is_active()) {
        ESP_LOGI(TAG, "event=knob direction=left action=volume_up");
        emit_event(MYBOT_KEY_EVENT_VOLUME_UP);
    }
}

static void knob_right(void *knob, void *user_data) {
    (void)knob;
    (void)user_data;
    if (adapter_is_active()) {
        ESP_LOGI(TAG, "event=knob direction=right action=volume_down");
        emit_event(MYBOT_KEY_EVENT_VOLUME_DOWN);
    }
}

static void button_task(void *argument) {
    (void)argument;
    bool stable_released = true;
    bool candidate_released = true;
    unsigned int candidate_samples = 0;
    unsigned int consecutive_failures = 0;
    bool long_press_emitted = false;
    TickType_t pressed_tick = 0;

    for (;;) {
        bool released = true;
        esp_err_t result = mybot_sensecap_read_knob_button(&released);
        if (result != ESP_OK) {
            candidate_samples = 0;
            ++consecutive_failures;
            if (consecutive_failures == 1 ||
                consecutive_failures % READ_FAILURE_LOG_INTERVAL == 0) {
                ESP_LOGW(TAG,
                         "event=button source=knob action=read result=error code=%s failures=%u",
                         esp_err_to_name(result), consecutive_failures);
            }
            vTaskDelay(pdMS_TO_TICKS(MYBOT_SENSECAP_BUTTON_POLL_INTERVAL_MS));
            continue;
        }
        if (consecutive_failures > 0) {
            ESP_LOGI(TAG, "event=button source=knob action=read result=recovered failures=%u",
                     consecutive_failures);
            consecutive_failures = 0;
        }

        if (released != candidate_released) {
            candidate_released = released;
            candidate_samples = 1;
        } else if (candidate_samples < MYBOT_SENSECAP_BUTTON_DEBOUNCE_SAMPLES) {
            ++candidate_samples;
        }

        TickType_t now = xTaskGetTickCount();
        if (candidate_samples >= MYBOT_SENSECAP_BUTTON_DEBOUNCE_SAMPLES &&
            candidate_released != stable_released) {
            stable_released = candidate_released;
            if (!stable_released) {
                pressed_tick = now;
                long_press_emitted = false;
            } else if (!long_press_emitted &&
                       now - pressed_tick >= pdMS_TO_TICKS(MYBOT_SENSECAP_BUTTON_LONG_PRESS_MS)) {
                long_press_emitted = true;
                handle_button_long_press();
            } else if (!long_press_emitted) {
                handle_button_short_press();
            }
        }
        if (!stable_released && !long_press_emitted &&
            now - pressed_tick >= pdMS_TO_TICKS(MYBOT_SENSECAP_BUTTON_LONG_PRESS_MS)) {
            long_press_emitted = true;
            handle_button_long_press();
        }

        vTaskDelay(pdMS_TO_TICKS(MYBOT_SENSECAP_BUTTON_POLL_INTERVAL_MS));
    }
}

static int wait_for_startup_release(void) {
    ESP_LOGI(TAG, "event=button source=knob action=wait_startup_release phase=begin");
    const TickType_t started = xTaskGetTickCount();
    unsigned int read_failures = 0;
    for (;;) {
        bool released = false;
        esp_err_t result = mybot_sensecap_read_knob_button(&released);
        if (result != ESP_OK) {
            ++read_failures;
            if (read_failures == 1) {
                ESP_LOGW(
                    TAG,
                    "event=button source=knob action=wait_startup_release result=retry code=%s",
                    esp_err_to_name(result));
            }
            if (read_failures >= STARTUP_RELEASE_READ_RETRIES) {
                ESP_LOGE(TAG,
                         "event=button source=knob action=wait_startup_release result=error "
                         "reason=read failures=%u",
                         read_failures);
                return -1;
            }
        } else {
            read_failures = 0;
        }
        if (result == ESP_OK && released) {
            ESP_LOGI(TAG, "event=button source=knob action=wait_startup_release result=ok");
            return 0;
        }
        if (xTaskGetTickCount() - started >= pdMS_TO_TICKS(STARTUP_RELEASE_TIMEOUT_MS)) {
            ESP_LOGE(TAG, "event=button source=knob action=wait_startup_release result=error "
                          "reason=timeout");
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void delete_failed_knob(knob_handle_t knob) {
    (void)iot_knob_unregister_cb(knob, KNOB_LEFT);
    (void)iot_knob_unregister_cb(knob, KNOB_RIGHT);
    (void)iot_knob_stop();
    vTaskDelay(pdMS_TO_TICKS(2 * CONFIG_KNOB_PERIOD_TIME_MS + 1));
    (void)iot_knob_delete(knob);
}

int mybot_sensecap_input_start(void) {
    portENTER_CRITICAL(&s_context.lock);
    bool already_started = s_context.knob || s_context.button_task;
    portEXIT_CRITICAL(&s_context.lock);
    if (already_started || wait_for_startup_release() < 0) {
        return -1;
    }

    const knob_config_t knob_config = {
        .default_direction = 0,
        .gpio_encoder_a = MYBOT_SENSECAP_KNOB_A,
        .gpio_encoder_b = MYBOT_SENSECAP_KNOB_B,
        .enable_power_save = false,
    };
    s_context.knob = iot_knob_create(&knob_config);
    if (!s_context.knob ||
        iot_knob_register_cb(s_context.knob, KNOB_LEFT, knob_left, NULL) != ESP_OK ||
        iot_knob_register_cb(s_context.knob, KNOB_RIGHT, knob_right, NULL) != ESP_OK ||
        xTaskCreate(button_task, "sensecap_btn", BUTTON_TASK_STACK_SIZE, NULL, BUTTON_TASK_PRIORITY,
                    &s_context.button_task) != pdPASS) {
        if (s_context.knob) {
            delete_failed_knob(s_context.knob);
            s_context.knob = NULL;
        }
        s_context.button_task = NULL;
        ESP_LOGE(TAG, "event=input action=initialize result=error");
        return -1;
    }

    ESP_LOGI(
        TAG,
        "event=input action=initialize result=ok knob_a=%d knob_b=%d button_source=tca9555_p0_3",
        MYBOT_SENSECAP_KNOB_A, MYBOT_SENSECAP_KNOB_B);
    return 0;
}

static int input_init(void **out_context, mybot_key_event_handler_t emit, void *user_data) {
    if (!out_context || !emit) {
        return -1;
    }
    *out_context = NULL;

    portENTER_CRITICAL(&s_context.lock);
    if (s_context.active || !s_context.knob || !s_context.button_task) {
        portEXIT_CRITICAL(&s_context.lock);
        return -1;
    }
    s_context.emit = emit;
    s_context.user_data = user_data;
    s_context.active = true;
    portEXIT_CRITICAL(&s_context.lock);

    *out_context = &s_context;
    ESP_LOGI(TAG, "event=sdk_adapter adapter=keys action=attach");
    return 0;
}

static void input_destroy(void *opaque) {
    if (opaque != &s_context) {
        return;
    }

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
    .init = input_init,
    .destroy = input_destroy,
};

const mybot_key_ops_t *mybot_sensecap_input_ops(void) {
    return &s_ops;
}
