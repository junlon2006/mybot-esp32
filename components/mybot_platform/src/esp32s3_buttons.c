/* SPDX-License-Identifier: Apache-2.0 */
#include "board_config.h"

#include <mybot/mybot.h>
#include <mybot/platform/mybot_key.h>

#include "button_gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_button.h"

#include <stdbool.h>

#define TAG "mybot_buttons"

typedef struct {
    portMUX_TYPE lock;
    bool active;
    unsigned int callbacks_in_flight;
    mybot_key_event_handler_t emit;
    void *user_data;
    button_handle_t boot;
    button_handle_t volume_up;
    button_handle_t volume_down;
} button_context_t;

static button_context_t s_context = {
    .lock = portMUX_INITIALIZER_UNLOCKED,
};

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

static void boot_click(void *button, void *user_data) {
    (void)button;
    (void)user_data;
    mybot_state_t state = mybot_get_state();
    if (state == MYBOT_STATE_READY) {
        emit_event(MYBOT_KEY_EVENT_CONVERSATION_START);
    } else if (state == MYBOT_STATE_IN_CONVERSATION) {
        emit_event(MYBOT_KEY_EVENT_CONVERSATION_STOP);
    }
}

static void boot_long_press(void *button, void *user_data) {
    (void)button;
    (void)user_data;
    emit_event(MYBOT_KEY_EVENT_PAIR);
}

static void volume_up_click(void *button, void *user_data) {
    (void)button;
    (void)user_data;
    emit_event(MYBOT_KEY_EVENT_VOLUME_UP);
}

static void volume_down_click(void *button, void *user_data) {
    (void)button;
    (void)user_data;
    emit_event(MYBOT_KEY_EVENT_VOLUME_DOWN);
}

static int create_button(gpio_num_t gpio, button_handle_t *handle) {
    button_config_t button_config = {
        .long_press_time = 3000,
        .short_press_time = 50,
    };
    button_gpio_config_t gpio_config = {
        .gpio_num = gpio,
        .active_level = 0,
        .enable_power_save = false,
        .disable_pull = false,
    };
    return iot_button_new_gpio_device(&button_config, &gpio_config, handle) == ESP_OK ? 0 : -1;
}

static void delete_buttons(void) {
    if (s_context.boot) {
        iot_button_delete(s_context.boot);
        s_context.boot = NULL;
    }
    if (s_context.volume_up) {
        iot_button_delete(s_context.volume_up);
        s_context.volume_up = NULL;
    }
    if (s_context.volume_down) {
        iot_button_delete(s_context.volume_down);
        s_context.volume_down = NULL;
    }
}

static int buttons_init(void **out_ctx, mybot_key_event_handler_t emit, void *user_data) {
    if (!out_ctx || !emit) {
        return -1;
    }
    *out_ctx = NULL;

    portENTER_CRITICAL(&s_context.lock);
    if (s_context.active) {
        portEXIT_CRITICAL(&s_context.lock);
        return -1;
    }
    s_context.emit = emit;
    s_context.user_data = user_data;
    portEXIT_CRITICAL(&s_context.lock);

    if (create_button(MYBOT_BOOT_BUTTON_GPIO, &s_context.boot) < 0 ||
        create_button(MYBOT_VOLUME_UP_BUTTON_GPIO, &s_context.volume_up) < 0 ||
        create_button(MYBOT_VOLUME_DOWN_BUTTON_GPIO, &s_context.volume_down) < 0 ||
        iot_button_register_cb(s_context.boot, BUTTON_SINGLE_CLICK, NULL, boot_click, NULL) !=
            ESP_OK ||
        iot_button_register_cb(s_context.boot, BUTTON_LONG_PRESS_START, NULL, boot_long_press,
                               NULL) != ESP_OK ||
        iot_button_register_cb(s_context.volume_up, BUTTON_SINGLE_CLICK, NULL, volume_up_click,
                               NULL) != ESP_OK ||
        iot_button_register_cb(s_context.volume_down, BUTTON_SINGLE_CLICK, NULL, volume_down_click,
                               NULL) != ESP_OK) {
        delete_buttons();
        portENTER_CRITICAL(&s_context.lock);
        s_context.emit = NULL;
        s_context.user_data = NULL;
        portEXIT_CRITICAL(&s_context.lock);
        return -1;
    }

    portENTER_CRITICAL(&s_context.lock);
    s_context.active = true;
    portEXIT_CRITICAL(&s_context.lock);
    *out_ctx = &s_context;
    return 0;
}

static void buttons_destroy(void *opaque) {
    if (opaque != &s_context) {
        return;
    }

    portENTER_CRITICAL(&s_context.lock);
    s_context.active = false;
    portEXIT_CRITICAL(&s_context.lock);
    delete_buttons();

    for (;;) {
        portENTER_CRITICAL(&s_context.lock);
        unsigned int in_flight = s_context.callbacks_in_flight;
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
    .name = "zhengchen-buttons",
    .init = buttons_init,
    .destroy = buttons_destroy,
};

int mybot_esp32s3_buttons_register(void) {
    int result = mybot_key_register(&s_ops);
    if (result < 0) {
        ESP_LOGE(TAG, "button registration failed");
    }
    return result;
}
