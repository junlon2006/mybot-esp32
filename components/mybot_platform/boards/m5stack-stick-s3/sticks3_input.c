/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#include "board_config.h"

#include <mybot/mybot.h>
#include <mybot/platform/mybot_key.h>

#include "board_actions.h"
#include "button_gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_button.h"

#include <stdbool.h>

#define TAG "sticks3_input"

typedef struct {
    portMUX_TYPE lock;
    bool active;
    unsigned int callbacks_in_flight;
    mybot_key_event_handler_t emit;
    void *user_data;
    button_handle_t boot;
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

static void handle_short_press(void) {
    if (!adapter_is_active()) {
        ESP_LOGI(TAG, "event=button source=main action=short_press result=ignored "
                      "reason=adapter_detached");
        return;
    }

    const mybot_state_t state = mybot_get_state();
    if (state == MYBOT_STATE_READY) {
        ESP_LOGI(TAG, "event=button source=main action=conversation_start");
        emit_event(MYBOT_KEY_EVENT_CONVERSATION_START);
    } else if (state == MYBOT_STATE_IN_CONVERSATION) {
        ESP_LOGI(TAG, "event=button source=main action=conversation_stop");
        emit_event(MYBOT_KEY_EVENT_CONVERSATION_STOP);
    } else {
        ESP_LOGI(TAG, "event=button source=main action=short_press result=ignored state=%d",
                 (int)state);
    }
}

static void handle_long_press(void) {
    ESP_LOGI(TAG, "event=button source=main action=wifi_provisioning_request");
    if (mybot_board_handle_boot_long_press() < 0) {
        ESP_LOGE(TAG, "event=provision_request source=main result=error");
    }
}

static void boot_click(void *button, void *user_data) {
    (void)button;
    (void)user_data;
    handle_short_press();
}

static void boot_long_press(void *button, void *user_data) {
    (void)button;
    (void)user_data;
    handle_long_press();
}

int mybot_sticks3_input_start(void) {
    portENTER_CRITICAL(&s_context.lock);
    bool already_started = s_context.boot != NULL;
    portEXIT_CRITICAL(&s_context.lock);
    if (already_started) {
        return -1;
    }

    const button_config_t button_config = {
        .long_press_time = MYBOT_STICKS3_BUTTON_LONG_PRESS_MS,
        .short_press_time = 50,
    };
    const button_gpio_config_t gpio_config = {
        .gpio_num = MYBOT_BOOT_BUTTON_GPIO,
        .active_level = 0,
        .enable_power_save = false,
        .disable_pull = false,
    };
    button_handle_t boot = NULL;
    if (iot_button_new_gpio_device(&button_config, &gpio_config, &boot) != ESP_OK ||
        iot_button_register_cb(boot, BUTTON_SINGLE_CLICK, NULL, boot_click, NULL) != ESP_OK ||
        iot_button_register_cb(boot, BUTTON_LONG_PRESS_START, NULL, boot_long_press, NULL) !=
            ESP_OK) {
        if (boot) {
            (void)iot_button_delete(boot);
        }
        ESP_LOGE(TAG, "event=input action=initialize result=error");
        return -1;
    }

    portENTER_CRITICAL(&s_context.lock);
    s_context.boot = boot;
    portEXIT_CRITICAL(&s_context.lock);
    ESP_LOGI(TAG, "event=input action=initialize result=ok main_gpio=%d active_level=low",
             MYBOT_BOOT_BUTTON_GPIO);
    return 0;
}

static int input_init(void **out_context, mybot_key_event_handler_t emit, void *user_data) {
    if (!out_context || !emit) {
        return -1;
    }
    *out_context = NULL;

    portENTER_CRITICAL(&s_context.lock);
    if (s_context.active || !s_context.boot) {
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

const mybot_key_ops_t *mybot_sticks3_input_ops(void) {
    return &s_ops;
}
