/* SPDX-License-Identifier: MIT */
#ifndef BREAD_MOCK_PLATFORM_H_
#define BREAD_MOCK_PLATFORM_H_

#include "host_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mock_button *button_handle_t;
typedef struct {
    uint32_t long_press_time;
    uint32_t short_press_time;
} button_config_t;
typedef struct {
    gpio_num_t gpio_num;
    int active_level;
    bool enable_power_save;
    bool disable_pull;
} button_gpio_config_t;
typedef enum {
    BUTTON_SINGLE_CLICK = 1,
    BUTTON_LONG_PRESS_START = 2,
} button_event_t;
typedef void (*button_callback_t)(void *button, void *user_data);

esp_err_t iot_button_new_gpio_device(const button_config_t *config,
                                     const button_gpio_config_t *gpio_config,
                                     button_handle_t *out_button);
esp_err_t iot_button_register_cb(button_handle_t button, button_event_t event, void *event_data,
                                 button_callback_t callback, void *user_data);
esp_err_t iot_button_delete(button_handle_t button);

#ifdef __cplusplus
}
#endif

#endif /* BREAD_MOCK_PLATFORM_H_ */
