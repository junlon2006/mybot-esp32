/* SPDX-License-Identifier: MIT */
#ifndef ATOM_ECHOS3R_MOCK_PLATFORM_H_
#define ATOM_ECHOS3R_MOCK_PLATFORM_H_

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
const char *esp_err_to_name(esp_err_t error);

typedef int gpio_num_t;
#define GPIO_NUM_0 0
#define GPIO_NUM_3 3
#define GPIO_NUM_4 4
#define GPIO_NUM_11 11
#define GPIO_NUM_17 17
#define GPIO_NUM_18 18
#define GPIO_NUM_41 41
#define GPIO_NUM_45 45
#define GPIO_NUM_48 48
#define GPIO_MODE_OUTPUT 1
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0

typedef struct {
    uint64_t pin_bit_mask;
    int mode;
    int pull_up_en;
    int pull_down_en;
    int intr_type;
} gpio_config_t;
esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level);
esp_err_t gpio_config(const gpio_config_t *config);
esp_err_t gpio_reset_pin(gpio_num_t pin);

#define I2C_NUM_0 0
#define I2C_CLK_SRC_DEFAULT 0
typedef struct mock_i2c_bus *i2c_master_bus_handle_t;
typedef struct {
    int i2c_port;
    int sda_io_num;
    int scl_io_num;
    int clk_source;
    int glitch_ignore_cnt;
    struct {
        unsigned int enable_internal_pullup : 1;
    } flags;
} i2c_master_bus_config_t;
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *config,
                             i2c_master_bus_handle_t *out_bus);
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t bus);

#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)

typedef pthread_mutex_t portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER
#define portENTER_CRITICAL(lock) ((void)pthread_mutex_lock(lock))
#define portEXIT_CRITICAL(lock) ((void)pthread_mutex_unlock(lock))
#define pdMS_TO_TICKS(ms) (ms)
void vTaskDelay(unsigned int ticks);

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

typedef enum {
    MYBOT_STATE_STARTING,
    MYBOT_STATE_READY,
    MYBOT_STATE_IN_CONVERSATION,
} mybot_state_t;
mybot_state_t mybot_get_state(void);

typedef enum {
    MYBOT_KEY_EVENT_CONVERSATION_START,
    MYBOT_KEY_EVENT_CONVERSATION_STOP,
} mybot_key_event_t;
typedef void (*mybot_key_event_handler_t)(mybot_key_event_t event, void *user_data);
typedef struct {
    int (*init)(void **out_context, mybot_key_event_handler_t emit, void *user_data);
    void (*destroy)(void *context);
} mybot_key_ops_t;

int mybot_board_handle_boot_long_press(void);

#endif /* ATOM_ECHOS3R_MOCK_PLATFORM_H_ */
