/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#include "board_config.h"

#include <mybot/mybot.h>
#include <mybot/platform/mybot_key.h>

#include "amoled175_hardware.h"
#include "board_actions.h"
#include "button_gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst9217.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "iot_button.h"

#include <stdbool.h>
#include <stdint.h>

#define TAG "amoled175_input"
#define INPUT_TASK_STACK_SIZE 3072
#define INPUT_TASK_PRIORITY 4

_Static_assert(MYBOT_AMOLED175_TOUCH_ADDRESS == ESP_LCD_TOUCH_IO_I2C_CST9217_ADDRESS,
               "CST9217 address mismatch");

typedef struct {
    portMUX_TYPE lock;
    esp_lcd_panel_io_handle_t touch_io;
    esp_lcd_touch_handle_t touch;
    button_handle_t boot;
    TaskHandle_t task;
    bool started;
    bool stop_requested;
    bool active;
    unsigned int callbacks_in_flight;
    mybot_key_event_handler_t emit;
    void *user_data;
} input_context_t;

static input_context_t s_context = {
    .lock = portMUX_INITIALIZER_UNLOCKED,
};
static StaticSemaphore_t s_task_stopped_storage;
static SemaphoreHandle_t s_task_stopped;

static bool adapter_is_active(void) {
    portENTER_CRITICAL(&s_context.lock);
    bool active = s_context.active;
    portEXIT_CRITICAL(&s_context.lock);
    return active;
}

static bool task_should_stop(void) {
    portENTER_CRITICAL(&s_context.lock);
    bool stop = s_context.stop_requested;
    portEXIT_CRITICAL(&s_context.lock);
    return stop;
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

static void handle_short_input(const char *source) {
    if (!adapter_is_active()) {
        ESP_LOGI(TAG,
                 "event=input source=%s action=short_press result=ignored "
                 "reason=adapter_detached",
                 source);
        return;
    }

    const mybot_state_t state = mybot_get_state();
    if (state == MYBOT_STATE_READY) {
        ESP_LOGI(TAG, "event=input source=%s action=conversation_start", source);
        emit_event(MYBOT_KEY_EVENT_CONVERSATION_START);
    } else if (state == MYBOT_STATE_IN_CONVERSATION) {
        ESP_LOGI(TAG, "event=input source=%s action=conversation_stop", source);
        emit_event(MYBOT_KEY_EVENT_CONVERSATION_STOP);
    } else {
        ESP_LOGI(TAG, "event=input source=%s action=short_press result=ignored state=%d", source,
                 (int)state);
    }
}

static void handle_long_input(const char *source) {
    ESP_LOGI(TAG, "event=input source=%s action=wifi_provisioning_request", source);
    if (mybot_board_handle_boot_long_press() < 0) {
        ESP_LOGE(TAG, "event=provision_request source=%s result=error", source);
    }
}

static void boot_click(void *button, void *user_data) {
    (void)button;
    (void)user_data;
    handle_short_input("boot_button");
}

static void boot_long_press(void *button, void *user_data) {
    (void)button;
    (void)user_data;
    handle_long_input("boot_button");
}

static void input_task(void *argument) {
    (void)argument;
    bool was_touched = false;
    bool long_press_sent = false;
    bool read_failed = false;
    TickType_t touch_started = 0;

    while (!task_should_stop()) {
        esp_err_t result = esp_lcd_touch_read_data(s_context.touch);
        if (task_should_stop()) {
            break;
        }

        bool touched = false;
        if (result == ESP_OK) {
            esp_lcd_touch_point_data_t point = {0};
            uint8_t point_count = 0;
            result = esp_lcd_touch_get_data(s_context.touch, &point, &point_count, 1);
            touched = result == ESP_OK && point_count > 0;
        }
        if (result == ESP_OK) {
            if (read_failed) {
                ESP_LOGI(TAG, "event=touch action=read result=recovered");
                read_failed = false;
            }
        } else {
            if (!read_failed) {
                ESP_LOGW(TAG, "event=touch action=read result=error code=%s",
                         esp_err_to_name(result));
                read_failed = true;
            }
            was_touched = false;
            long_press_sent = false;
        }

        const TickType_t now = xTaskGetTickCount();
        if (touched && !was_touched) {
            was_touched = true;
            long_press_sent = false;
            touch_started = now;
        } else if (touched && !long_press_sent &&
                   now - touch_started >= pdMS_TO_TICKS(MYBOT_AMOLED175_INPUT_LONG_PRESS_MS)) {
            long_press_sent = true;
            handle_long_input("touch");
        } else if (!touched && was_touched) {
            const TickType_t duration = now - touch_started;
            was_touched = false;
            if (!long_press_sent &&
                duration >= pdMS_TO_TICKS(MYBOT_AMOLED175_INPUT_SHORT_PRESS_MIN_MS)) {
                handle_short_input("touch");
            }
            long_press_sent = false;
        }

        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(MYBOT_AMOLED175_INPUT_POLL_INTERVAL_MS));
    }

    portENTER_CRITICAL(&s_context.lock);
    s_context.task = NULL;
    portEXIT_CRITICAL(&s_context.lock);
    xSemaphoreGive(s_task_stopped);
    vTaskDelete(NULL);
}

static int release_input_devices(void) {
    int result = 0;

    if (s_context.boot) {
        esp_err_t err = iot_button_delete(s_context.boot);
        if (err == ESP_OK) {
            s_context.boot = NULL;
        } else {
            ESP_LOGW(TAG, "event=input action=cleanup component=boot_button result=error code=%s",
                     esp_err_to_name(err));
            result = -1;
        }
    }
    if (s_context.touch) {
        esp_err_t err = esp_lcd_touch_del(s_context.touch);
        if (err == ESP_OK) {
            s_context.touch = NULL;
        } else {
            ESP_LOGW(TAG, "event=input action=cleanup component=touch result=error code=%s",
                     esp_err_to_name(err));
            result = -1;
        }
    }
    if (!s_context.touch && s_context.touch_io) {
        esp_err_t err = esp_lcd_panel_io_del(s_context.touch_io);
        if (err == ESP_OK) {
            s_context.touch_io = NULL;
        } else {
            ESP_LOGW(TAG, "event=input action=cleanup component=touch_io result=error code=%s",
                     esp_err_to_name(err));
            result = -1;
        }
    }
    return result;
}

int mybot_amoled175_input_stop(void) {
    portENTER_CRITICAL(&s_context.lock);
    s_context.active = false;
    s_context.stop_requested = true;
    TaskHandle_t task = s_context.task;
    unsigned int in_flight = s_context.callbacks_in_flight;
    portEXIT_CRITICAL(&s_context.lock);

    if (task) {
        if (!s_task_stopped || xSemaphoreTake(s_task_stopped, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "event=input action=deinitialize result=error reason=task_join");
            return -1;
        }
    }
    while (in_flight > 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
        portENTER_CRITICAL(&s_context.lock);
        in_flight = s_context.callbacks_in_flight;
        portEXIT_CRITICAL(&s_context.lock);
    }

    const int result = release_input_devices();
    portENTER_CRITICAL(&s_context.lock);
    s_context.emit = NULL;
    s_context.user_data = NULL;
    s_context.started = s_context.boot || s_context.touch || s_context.touch_io;
    if (!s_context.started) {
        s_context.stop_requested = false;
    }
    portEXIT_CRITICAL(&s_context.lock);

    ESP_LOGI(TAG, "event=input action=deinitialize result=%s", result == 0 ? "ok" : "error");
    return result;
}

int mybot_amoled175_input_start(void) {
    portENTER_CRITICAL(&s_context.lock);
    const bool already_started = s_context.started || s_context.task || s_context.boot ||
                                 s_context.touch || s_context.touch_io;
    portEXIT_CRITICAL(&s_context.lock);
    if (already_started) {
        return -1;
    }

    i2c_master_bus_handle_t bus = mybot_amoled175_i2c_bus_handle();
    if (!bus) {
        return -1;
    }
    if (!s_task_stopped) {
        s_task_stopped = xSemaphoreCreateBinaryStatic(&s_task_stopped_storage);
    }
    if (!s_task_stopped) {
        return -1;
    }
    (void)xSemaphoreTake(s_task_stopped, 0);

    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_CST9217_CONFIG();
    io_config.scl_speed_hz = MYBOT_AMOLED175_I2C_SPEED_HZ;
    esp_err_t result = esp_lcd_new_panel_io_i2c(bus, &io_config, &s_context.touch_io);

    const esp_lcd_touch_config_t touch_config = {
        .x_max = MYBOT_DISPLAY_WIDTH - 1,
        .y_max = MYBOT_DISPLAY_HEIGHT - 1,
        .rst_gpio_num = MYBOT_AMOLED175_TOUCH_RESET,
        .int_gpio_num = MYBOT_AMOLED175_TOUCH_INTERRUPT,
        .levels =
            {
                .reset = 0,
                .interrupt = 0,
            },
        .flags =
            {
                .swap_xy = 0,
                .mirror_x = 1,
                .mirror_y = 1,
            },
    };
    if (result == ESP_OK) {
        result = esp_lcd_touch_new_i2c_cst9217(s_context.touch_io, &touch_config, &s_context.touch);
    }

    const button_config_t button_config = {
        .long_press_time = MYBOT_AMOLED175_INPUT_LONG_PRESS_MS,
        .short_press_time = MYBOT_AMOLED175_INPUT_SHORT_PRESS_MIN_MS,
    };
    const button_gpio_config_t gpio_config = {
        .gpio_num = MYBOT_BOOT_BUTTON_GPIO,
        .active_level = 0,
        .enable_power_save = false,
        .disable_pull = false,
    };
    if (result == ESP_OK) {
        result = iot_button_new_gpio_device(&button_config, &gpio_config, &s_context.boot);
    }
    if (result == ESP_OK) {
        result =
            iot_button_register_cb(s_context.boot, BUTTON_SINGLE_CLICK, NULL, boot_click, NULL);
    }
    if (result == ESP_OK) {
        result = iot_button_register_cb(s_context.boot, BUTTON_LONG_PRESS_START, NULL,
                                        boot_long_press, NULL);
    }
    if (result == ESP_OK && xTaskCreate(input_task, "amoled175_input", INPUT_TASK_STACK_SIZE, NULL,
                                        INPUT_TASK_PRIORITY, &s_context.task) != pdPASS) {
        result = ESP_ERR_NO_MEM;
    }
    if (result != ESP_OK) {
        if (release_input_devices() < 0) {
            ESP_LOGE(TAG, "event=input action=initialize cleanup=error");
        }
        ESP_LOGE(TAG, "event=input action=initialize result=error code=%s",
                 esp_err_to_name(result));
        return -1;
    }

    portENTER_CRITICAL(&s_context.lock);
    s_context.started = true;
    s_context.stop_requested = false;
    portEXIT_CRITICAL(&s_context.lock);
    ESP_LOGI(TAG,
             "event=input action=initialize result=ok touch=cst9217 touch_address=0x%02x "
             "boot_gpio=%d poll_ms=%d",
             MYBOT_AMOLED175_TOUCH_ADDRESS, MYBOT_BOOT_BUTTON_GPIO,
             MYBOT_AMOLED175_INPUT_POLL_INTERVAL_MS);
    return 0;
}

static int input_init(void **out_context, mybot_key_event_handler_t emit, void *user_data) {
    if (!out_context || !emit) {
        return -1;
    }
    *out_context = NULL;

    portENTER_CRITICAL(&s_context.lock);
    if (!s_context.started || s_context.active) {
        portEXIT_CRITICAL(&s_context.lock);
        return -1;
    }
    s_context.emit = emit;
    s_context.user_data = user_data;
    s_context.active = true;
    portEXIT_CRITICAL(&s_context.lock);

    *out_context = &s_context;
    ESP_LOGI(TAG, "event=sdk_adapter adapter=keys action=attach sources=boot_button,touch");
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

    while (in_flight > 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
        portENTER_CRITICAL(&s_context.lock);
        in_flight = s_context.callbacks_in_flight;
        portEXIT_CRITICAL(&s_context.lock);
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

const mybot_key_ops_t *mybot_amoled175_input_ops(void) {
    return &s_ops;
}
