/* SPDX-License-Identifier: Apache-2.0 */
#include "board_config.h"

#include <mybot/mybot.h>
#include <mybot/platform/mybot_key.h>

#include "button_gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "iot_button.h"
#include "board_actions.h"

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
    StaticSemaphore_t cleanup_done_storage;
    SemaphoreHandle_t cleanup_done;
    esp_timer_handle_t cleanup_timer;
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

    portENTER_CRITICAL(&s_context.lock);
    bool active = s_context.active;
    portEXIT_CRITICAL(&s_context.lock);
    if (!active) {
        return;
    }

    ESP_LOGI(TAG, "Boot button long pressed");
    if (mybot_board_handle_boot_long_press() < 0) {
        ESP_LOGE(TAG, "Boot long-press action failed");
        return;
    }
    emit_event(MYBOT_KEY_EVENT_EXIT);
}

static void volume_up_click(void *button, void *user_data) {
    (void)button;
    (void)user_data;
    ESP_LOGI(TAG, "volume up button pressed");
    emit_event(MYBOT_KEY_EVENT_VOLUME_UP);
}

static void volume_down_click(void *button, void *user_data) {
    (void)button;
    (void)user_data;
    ESP_LOGI(TAG, "volume down button pressed");
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

static esp_err_t delete_buttons(void) {
    esp_err_t result = ESP_OK;
    if (s_context.boot) {
        esp_err_t err = iot_button_delete(s_context.boot);
        if (err == ESP_OK) {
            s_context.boot = NULL;
        } else {
            result = err;
        }
    }
    if (s_context.volume_up) {
        esp_err_t err = iot_button_delete(s_context.volume_up);
        if (err == ESP_OK) {
            s_context.volume_up = NULL;
        } else {
            result = err;
        }
    }
    if (s_context.volume_down) {
        esp_err_t err = iot_button_delete(s_context.volume_down);
        if (err == ESP_OK) {
            s_context.volume_down = NULL;
        } else {
            result = err;
        }
    }
    return result;
}

static void cleanup_buttons_callback(void *opaque) {
    button_context_t *context = opaque;
    esp_timer_handle_t timer = context->cleanup_timer;
    ESP_ERROR_CHECK(delete_buttons());
    ESP_ERROR_CHECK(esp_timer_delete(timer));
    context->cleanup_timer = NULL;
    xSemaphoreGive(context->cleanup_done);
}

static int delete_buttons_synchronously(void) {
    if (!s_context.cleanup_timer || !s_context.cleanup_done) {
        return s_context.boot || s_context.volume_up || s_context.volume_down ? -1 : 0;
    }

    (void)xSemaphoreTake(s_context.cleanup_done, 0);
    ESP_ERROR_CHECK(esp_timer_start_once(s_context.cleanup_timer, 1));
    xSemaphoreTake(s_context.cleanup_done, portMAX_DELAY);
    return 0;
}

static int buttons_init(void **out_ctx, mybot_key_event_handler_t emit, void *user_data) {
    if (!out_ctx || !emit) {
        return -1;
    }
    *out_ctx = NULL;

    portENTER_CRITICAL(&s_context.lock);
    if (s_context.active || s_context.boot || s_context.volume_up || s_context.volume_down ||
        s_context.cleanup_timer) {
        portEXIT_CRITICAL(&s_context.lock);
        return -1;
    }
    s_context.emit = emit;
    s_context.user_data = user_data;
    portEXIT_CRITICAL(&s_context.lock);

    if (!s_context.cleanup_done) {
        s_context.cleanup_done = xSemaphoreCreateBinaryStatic(&s_context.cleanup_done_storage);
    }
    const esp_timer_create_args_t cleanup_timer_args = {
        .callback = cleanup_buttons_callback,
        .arg = &s_context,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "button_cleanup",
    };
    if (!s_context.cleanup_done ||
        esp_timer_create(&cleanup_timer_args, &s_context.cleanup_timer) != ESP_OK) {
        portENTER_CRITICAL(&s_context.lock);
        s_context.emit = NULL;
        s_context.user_data = NULL;
        portEXIT_CRITICAL(&s_context.lock);
        return -1;
    }

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
        if (delete_buttons_synchronously() < 0) {
            ESP_LOGE(TAG, "failed to drain button event source after initialization error");
        }
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
    if (delete_buttons_synchronously() < 0) {
        ESP_LOGE(TAG, "failed to drain button event source");
    }

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
    .init = buttons_init,
    .destroy = buttons_destroy,
};

const mybot_key_ops_t *mybot_esp32s3_button_ops(void) {
    return &s_ops;
}
