/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd. */
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
#include "xvf3800_hardware.h"

#include <stdbool.h>

#define TAG "xvf3800_buttons"
#define XVF3800_BUTTON_TASK_STACK_SIZE 4096
#define XVF3800_BUTTON_TASK_PRIORITY 4
#define XVF3800_READ_FAILURE_LOG_INTERVAL 100

typedef struct {
    portMUX_TYPE lock;
    bool active;
    unsigned int callbacks_in_flight;
    mybot_key_event_handler_t emit;
    void *user_data;
    button_handle_t boot;
    TaskHandle_t xvf3800_task;
} button_context_t;

static button_context_t s_context = {
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

static void handle_short_press(const char *source) {
    if (!adapter_is_active()) {
        ESP_LOGI(TAG,
                 "event=button source=%s action=short_press result=ignored reason=adapter_detached",
                 source);
        return;
    }

    mybot_state_t state = mybot_get_state();
    if (state == MYBOT_STATE_READY) {
        ESP_LOGI(TAG, "event=button source=%s action=conversation_start", source);
        emit_event(MYBOT_KEY_EVENT_CONVERSATION_START);
    } else if (state == MYBOT_STATE_IN_CONVERSATION) {
        ESP_LOGI(TAG, "event=button source=%s action=conversation_stop", source);
        emit_event(MYBOT_KEY_EVENT_CONVERSATION_STOP);
    } else {
        ESP_LOGI(TAG, "event=button source=%s action=short_press result=ignored state=%d", source,
                 (int)state);
    }
}

static void handle_long_press(const char *source) {
    ESP_LOGI(TAG, "event=button source=%s action=wifi_provisioning_request", source);
    if (mybot_board_handle_boot_long_press() < 0) {
        ESP_LOGE(TAG, "event=provision_request source=%s result=error", source);
    }
}

static void boot_click(void *button, void *user_data) {
    (void)button;
    (void)user_data;
    handle_short_press("boot");
}

static void boot_long_press(void *button, void *user_data) {
    (void)button;
    (void)user_data;
    handle_long_press("boot");
}

static int create_boot_button(void) {
    const button_config_t button_config = {
        .long_press_time = MYBOT_XVF3800_BUTTON_LONG_PRESS_MS,
        .short_press_time = 50,
    };
    const button_gpio_config_t gpio_config = {
        .gpio_num = MYBOT_BOOT_BUTTON_GPIO,
        .active_level = 0,
        .enable_power_save = false,
        .disable_pull = false,
    };
    if (iot_button_new_gpio_device(&button_config, &gpio_config, &s_context.boot) != ESP_OK) {
        return -1;
    }
    if (iot_button_register_cb(s_context.boot, BUTTON_SINGLE_CLICK, NULL, boot_click, NULL) !=
            ESP_OK ||
        iot_button_register_cb(s_context.boot, BUTTON_LONG_PRESS_START, NULL, boot_long_press,
                               NULL) != ESP_OK) {
        (void)iot_button_delete(s_context.boot);
        s_context.boot = NULL;
        return -1;
    }
    return 0;
}

static void xvf3800_button_task(void *argument) {
    (void)argument;
    bool have_last_state = false;
    bool last_released = true;
    bool long_press_emitted = false;
    TickType_t press_tick = 0;
    unsigned int consecutive_failures = 0;
    unsigned int consecutive_retries = 0;

    vTaskDelay(pdMS_TO_TICKS(500));
    for (;;) {
        bool released = true;
        esp_err_t result = mybot_xvf3800_read_button(&released);
        if (result == ESP_ERR_NOT_FINISHED) {
            ++consecutive_retries;
            if (consecutive_retries == 1 ||
                consecutive_retries % XVF3800_READ_FAILURE_LOG_INTERVAL == 0) {
                ESP_LOGI(TAG,
                         "event=button source=xvf3800_gpi0 action=read result=retry retries=%u",
                         consecutive_retries);
            }
            vTaskDelay(pdMS_TO_TICKS(MYBOT_XVF3800_BUTTON_POLL_INTERVAL_MS));
            continue;
        }
        if (result != ESP_OK) {
            ++consecutive_failures;
            if (consecutive_failures == 1 ||
                consecutive_failures % XVF3800_READ_FAILURE_LOG_INTERVAL == 0) {
                ESP_LOGW(TAG,
                         "event=button source=xvf3800_gpi0 action=read result=error code=%s "
                         "failures=%u",
                         esp_err_to_name(result), consecutive_failures);
            }
            vTaskDelay(pdMS_TO_TICKS(MYBOT_XVF3800_BUTTON_POLL_INTERVAL_MS));
            continue;
        }
        if (consecutive_retries > 0) {
            ESP_LOGI(TAG, "event=button source=xvf3800_gpi0 action=read result=ready retries=%u",
                     consecutive_retries);
            consecutive_retries = 0;
        }
        if (consecutive_failures > 0) {
            ESP_LOGI(TAG,
                     "event=button source=xvf3800_gpi0 action=read result=recovered failures=%u",
                     consecutive_failures);
            consecutive_failures = 0;
        }

        TickType_t now = xTaskGetTickCount();
        if (!have_last_state) {
            have_last_state = true;
            last_released = released;
            if (!released) {
                press_tick = now;
            }
            ESP_LOGI(TAG, "event=button source=xvf3800_gpi0 action=initialize state=%s",
                     released ? "released" : "pressed");
        } else if (last_released && !released) {
            press_tick = now;
            long_press_emitted = false;
        } else if (!released && !long_press_emitted &&
                   now - press_tick >= pdMS_TO_TICKS(MYBOT_XVF3800_BUTTON_LONG_PRESS_MS)) {
            long_press_emitted = true;
            handle_long_press("xvf3800_gpi0");
        } else if (!last_released && released) {
            if (!long_press_emitted &&
                now - press_tick >= pdMS_TO_TICKS(MYBOT_XVF3800_BUTTON_LONG_PRESS_MS)) {
                long_press_emitted = true;
                handle_long_press("xvf3800_gpi0");
            } else if (!long_press_emitted) {
                handle_short_press("xvf3800_gpi0");
            }
        }
        last_released = released;
        vTaskDelay(pdMS_TO_TICKS(MYBOT_XVF3800_BUTTON_POLL_INTERVAL_MS));
    }
}

int mybot_xvf3800_buttons_start(void) {
    portENTER_CRITICAL(&s_context.lock);
    bool already_started = s_context.boot || s_context.xvf3800_task;
    portEXIT_CRITICAL(&s_context.lock);
    if (already_started) {
        return -1;
    }

    if (create_boot_button() < 0 ||
        xTaskCreate(xvf3800_button_task, "xvf3800_btn", XVF3800_BUTTON_TASK_STACK_SIZE, NULL,
                    XVF3800_BUTTON_TASK_PRIORITY, &s_context.xvf3800_task) != pdPASS) {
        if (s_context.boot) {
            (void)iot_button_delete(s_context.boot);
            s_context.boot = NULL;
        }
        s_context.xvf3800_task = NULL;
        ESP_LOGE(TAG, "event=buttons action=initialize result=error");
        return -1;
    }

    ESP_LOGI(TAG, "event=buttons action=initialize result=ok boot_gpio=%d xvf_poll_ms=%d",
             MYBOT_BOOT_BUTTON_GPIO, MYBOT_XVF3800_BUTTON_POLL_INTERVAL_MS);
    return 0;
}

static int buttons_init(void **out_ctx, mybot_key_event_handler_t emit, void *user_data) {
    if (!out_ctx || !emit) {
        return -1;
    }
    *out_ctx = NULL;

    portENTER_CRITICAL(&s_context.lock);
    if (s_context.active || !s_context.boot || !s_context.xvf3800_task) {
        portEXIT_CRITICAL(&s_context.lock);
        return -1;
    }
    s_context.emit = emit;
    s_context.user_data = user_data;
    s_context.active = true;
    portEXIT_CRITICAL(&s_context.lock);

    *out_ctx = &s_context;
    ESP_LOGI(TAG, "event=sdk_adapter adapter=keys action=attach");
    return 0;
}

static void buttons_destroy(void *opaque) {
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
    .init = buttons_init,
    .destroy = buttons_destroy,
};

const mybot_key_ops_t *mybot_xvf3800_button_ops(void) {
    return &s_ops;
}
