/* SPDX-License-Identifier: MIT */
#include "mock_platform.h"

#include "atom_echos3r_hardware.h"
#include "board_config.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

struct mock_i2c_bus {
    int unused;
};

struct mock_button {
    bool open;
    button_callback_t click;
    button_callback_t long_press;
    void *click_user_data;
    void *long_press_user_data;
};

static struct mock_i2c_bus g_bus;
static struct mock_button g_button;
static bool g_bus_open;
static bool g_pa_configured;
static int g_pa_level;
static bool g_fail_set_level;
static bool g_fail_gpio_config;
static bool g_fail_gpio_reset;
static bool g_fail_i2c_new;
static bool g_fail_i2c_new_with_partial_bus;
static bool g_fail_i2c_delete;
static bool g_fail_button_new;
static button_event_t g_fail_button_register_event;
static unsigned int g_i2c_new_calls;
static unsigned int g_i2c_delete_calls;
static unsigned int g_gpio_config_calls;
static unsigned int g_button_delete_calls;
static unsigned int g_state_calls;
static unsigned int g_provision_calls;
static mybot_state_t g_state;
static atomic_uint g_delay_calls;

const char *esp_err_to_name(esp_err_t error) {
    return error == ESP_OK ? "ESP_OK" : "ESP_FAIL";
}

esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level) {
    CHECK(pin == MYBOT_ATOM_ECHOS3R_PA_ENABLE);
    CHECK(level <= 1);
    if (g_fail_set_level) {
        g_fail_set_level = false;
        return ESP_FAIL;
    }
    g_pa_level = (int)level;
    return ESP_OK;
}

esp_err_t gpio_config(const gpio_config_t *config) {
    ++g_gpio_config_calls;
    CHECK(config->pin_bit_mask == (1ULL << MYBOT_ATOM_ECHOS3R_PA_ENABLE));
    CHECK(config->mode == GPIO_MODE_OUTPUT);
    if (g_fail_gpio_config) {
        g_fail_gpio_config = false;
        return ESP_FAIL;
    }
    CHECK(!g_pa_configured);
    g_pa_configured = true;
    return ESP_OK;
}

esp_err_t gpio_reset_pin(gpio_num_t pin) {
    CHECK(pin == MYBOT_ATOM_ECHOS3R_PA_ENABLE);
    if (g_fail_gpio_reset) {
        g_fail_gpio_reset = false;
        return ESP_FAIL;
    }
    CHECK(g_pa_configured);
    g_pa_configured = false;
    return ESP_OK;
}

esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *config,
                             i2c_master_bus_handle_t *out_bus) {
    ++g_i2c_new_calls;
    CHECK(config->i2c_port == MYBOT_ATOM_ECHOS3R_I2C_PORT);
    CHECK(config->sda_io_num == MYBOT_ATOM_ECHOS3R_I2C_SDA);
    CHECK(config->scl_io_num == MYBOT_ATOM_ECHOS3R_I2C_SCL);
    CHECK(config->flags.enable_internal_pullup == 1);
    CHECK(!g_bus_open);
    if (g_fail_i2c_new) {
        g_fail_i2c_new = false;
        if (g_fail_i2c_new_with_partial_bus) {
            g_fail_i2c_new_with_partial_bus = false;
            g_bus_open = true;
            *out_bus = &g_bus;
        }
        return ESP_FAIL;
    }
    g_bus_open = true;
    *out_bus = &g_bus;
    return ESP_OK;
}

esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t bus) {
    ++g_i2c_delete_calls;
    CHECK(bus == &g_bus);
    CHECK(g_bus_open);
    if (g_fail_i2c_delete) {
        g_fail_i2c_delete = false;
        return ESP_FAIL;
    }
    g_bus_open = false;
    return ESP_OK;
}

void vTaskDelay(unsigned int ticks) {
    atomic_fetch_add(&g_delay_calls, 1);
    struct timespec delay = {
        .tv_sec = ticks / 1000,
        .tv_nsec = (long)(ticks % 1000) * 1000000L,
    };
    nanosleep(&delay, NULL);
}

esp_err_t iot_button_new_gpio_device(const button_config_t *config,
                                     const button_gpio_config_t *gpio_config,
                                     button_handle_t *out_button) {
    CHECK(config->long_press_time == 3000);
    CHECK(config->short_press_time == 50);
    CHECK(gpio_config->gpio_num == MYBOT_BOOT_BUTTON_GPIO);
    CHECK(gpio_config->active_level == 0);
    CHECK(!g_button.open);
    if (g_fail_button_new) {
        g_fail_button_new = false;
        return ESP_FAIL;
    }
    g_button.open = true;
    *out_button = &g_button;
    return ESP_OK;
}

esp_err_t iot_button_register_cb(button_handle_t button, button_event_t event, void *event_data,
                                 button_callback_t callback, void *user_data) {
    CHECK(button == &g_button && button->open);
    CHECK(event_data == NULL && callback != NULL);
    if (g_fail_button_register_event == event) {
        g_fail_button_register_event = 0;
        return ESP_FAIL;
    }
    if (event == BUTTON_SINGLE_CLICK) {
        button->click = callback;
        button->click_user_data = user_data;
    } else if (event == BUTTON_LONG_PRESS_START) {
        button->long_press = callback;
        button->long_press_user_data = user_data;
    } else {
        CHECK(false);
    }
    return ESP_OK;
}

esp_err_t iot_button_delete(button_handle_t button) {
    CHECK(button == &g_button && button->open);
    ++g_button_delete_calls;
    button->open = false;
    button->click = NULL;
    button->long_press = NULL;
    return ESP_OK;
}

mybot_state_t mybot_get_state(void) {
    ++g_state_calls;
    return g_state;
}

int mybot_board_handle_boot_long_press(void) {
    ++g_provision_calls;
    return 0;
}

int mybot_atom_echos3r_input_start(void);
const mybot_key_ops_t *mybot_atom_echos3r_input_ops(void);

static void fire_button(button_event_t event) {
    CHECK(g_button.open);
    button_callback_t callback =
        event == BUTTON_SINGLE_CLICK ? g_button.click : g_button.long_press;
    void *user_data =
        event == BUTTON_SINGLE_CLICK ? g_button.click_user_data : g_button.long_press_user_data;
    CHECK(callback != NULL);
    callback(&g_button, user_data);
}

static void assert_hardware_released(void) {
    CHECK(mybot_atom_echos3r_i2c_bus_handle() == NULL);
    CHECK(!g_bus_open);
    CHECK(!g_pa_configured);
    CHECK(g_pa_level == 0);
    CHECK(mybot_atom_echos3r_set_speaker_power(true) < 0);
}

static void test_hardware_failures_and_retry(void) {
    CHECK(mybot_atom_echos3r_hardware_deinit() == 0);
    assert_hardware_released();

    g_fail_set_level = true;
    CHECK(mybot_atom_echos3r_hardware_init() < 0);
    assert_hardware_released();

    g_fail_gpio_config = true;
    CHECK(mybot_atom_echos3r_hardware_init() < 0);
    assert_hardware_released();

    g_fail_i2c_new = true;
    CHECK(mybot_atom_echos3r_hardware_init() < 0);
    assert_hardware_released();

    g_fail_i2c_new = true;
    g_fail_i2c_new_with_partial_bus = true;
    unsigned int deletes_before = g_i2c_delete_calls;
    CHECK(mybot_atom_echos3r_hardware_init() < 0);
    CHECK(g_i2c_delete_calls == deletes_before + 1);
    assert_hardware_released();

    CHECK(mybot_atom_echos3r_hardware_init() == 0);
    CHECK(g_pa_configured && g_bus_open && g_pa_level == 0);
    CHECK(mybot_atom_echos3r_i2c_bus_handle() == &g_bus);
    unsigned int inits_before = g_i2c_new_calls;
    CHECK(mybot_atom_echos3r_hardware_init() == 0);
    CHECK(g_i2c_new_calls == inits_before);

    CHECK(mybot_atom_echos3r_set_speaker_power(true) == 0);
    CHECK(g_pa_level == 1);
    g_fail_set_level = true;
    CHECK(mybot_atom_echos3r_set_speaker_power(false) < 0);
    CHECK(g_pa_level == 1);
    CHECK(mybot_atom_echos3r_set_speaker_power(false) == 0);
    CHECK(g_pa_level == 0);

    g_fail_i2c_delete = true;
    CHECK(mybot_atom_echos3r_hardware_deinit() < 0);
    CHECK(g_bus_open && !g_pa_configured);
    CHECK(mybot_atom_echos3r_i2c_bus_handle() == NULL);

    g_fail_i2c_delete = true;
    inits_before = g_i2c_new_calls;
    CHECK(mybot_atom_echos3r_hardware_init() < 0);
    CHECK(g_i2c_new_calls == inits_before);
    CHECK(g_bus_open);
    CHECK(mybot_atom_echos3r_hardware_init() == 0);
    CHECK(g_bus_open && g_pa_configured);

    g_fail_gpio_reset = true;
    CHECK(mybot_atom_echos3r_hardware_deinit() < 0);
    CHECK(!g_bus_open && g_pa_configured);
    CHECK(mybot_atom_echos3r_hardware_deinit() == 0);
    assert_hardware_released();
    CHECK(mybot_atom_echos3r_hardware_deinit() == 0);
    CHECK(g_gpio_config_calls >= 5);
}

static unsigned int g_event_count;
static mybot_key_event_t g_last_event;

static void record_event(mybot_key_event_t event, void *user_data) {
    CHECK(user_data == &g_event_count);
    ++g_event_count;
    g_last_event = event;
}

static pthread_mutex_t g_callback_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_callback_cond = PTHREAD_COND_INITIALIZER;
static bool g_callback_entered;
static bool g_callback_release;
static atomic_bool g_destroy_done;

static void blocking_event(mybot_key_event_t event, void *user_data) {
    CHECK(event == MYBOT_KEY_EVENT_CONVERSATION_START);
    CHECK(user_data == &g_callback_entered);
    pthread_mutex_lock(&g_callback_lock);
    g_callback_entered = true;
    pthread_cond_broadcast(&g_callback_cond);
    while (!g_callback_release) {
        pthread_cond_wait(&g_callback_cond, &g_callback_lock);
    }
    pthread_mutex_unlock(&g_callback_lock);
}

static void wait_for_callback_entry(void) {
    struct timespec deadline;
    CHECK(clock_gettime(CLOCK_REALTIME, &deadline) == 0);
    deadline.tv_sec += 2;
    pthread_mutex_lock(&g_callback_lock);
    while (!g_callback_entered) {
        int result = pthread_cond_timedwait(&g_callback_cond, &g_callback_lock, &deadline);
        CHECK(result != ETIMEDOUT);
        CHECK(result == 0);
    }
    pthread_mutex_unlock(&g_callback_lock);
}

static void *click_thread(void *argument) {
    (void)argument;
    fire_button(BUTTON_SINGLE_CLICK);
    return NULL;
}

static void *destroy_thread(void *argument) {
    const mybot_key_ops_t *ops = mybot_atom_echos3r_input_ops();
    ops->destroy(argument);
    atomic_store(&g_destroy_done, true);
    return NULL;
}

static void test_input_lifecycle(void) {
    const mybot_key_ops_t *ops = mybot_atom_echos3r_input_ops();
    void *context = NULL;
    CHECK(ops != NULL && ops->init != NULL && ops->destroy != NULL);
    CHECK(ops->init(&context, record_event, &g_event_count) < 0);
    CHECK(context == NULL);

    g_fail_button_new = true;
    CHECK(mybot_atom_echos3r_input_start() < 0);
    CHECK(!g_button.open);
    g_fail_button_register_event = BUTTON_LONG_PRESS_START;
    unsigned int deletes_before = g_button_delete_calls;
    CHECK(mybot_atom_echos3r_input_start() < 0);
    CHECK(g_button_delete_calls == deletes_before + 1);
    CHECK(!g_button.open);

    CHECK(mybot_atom_echos3r_input_start() == 0);
    CHECK(g_button.open);
    CHECK(mybot_atom_echos3r_input_start() < 0);
    CHECK(ops->init(NULL, record_event, &g_event_count) < 0);
    CHECK(ops->init(&context, NULL, &g_event_count) < 0);

    for (unsigned int cycle = 0; cycle < 3; ++cycle) {
        CHECK(ops->init(&context, record_event, &g_event_count) == 0);
        CHECK(context != NULL);
        void *duplicate = NULL;
        CHECK(ops->init(&duplicate, record_event, &g_event_count) < 0);
        CHECK(duplicate == NULL);

        g_state = MYBOT_STATE_READY;
        fire_button(BUTTON_SINGLE_CLICK);
        CHECK(g_event_count == 2 * cycle + 1);
        CHECK(g_last_event == MYBOT_KEY_EVENT_CONVERSATION_START);
        g_state = MYBOT_STATE_IN_CONVERSATION;
        fire_button(BUTTON_SINGLE_CLICK);
        CHECK(g_event_count == 2 * cycle + 2);
        CHECK(g_last_event == MYBOT_KEY_EVENT_CONVERSATION_STOP);
        g_state = MYBOT_STATE_STARTING;
        fire_button(BUTTON_SINGLE_CLICK);
        CHECK(g_event_count == 2 * cycle + 2);

        unsigned int provisions_before = g_provision_calls;
        fire_button(BUTTON_LONG_PRESS_START);
        CHECK(g_provision_calls == provisions_before + 1);
        ops->destroy(context);
        context = NULL;
        unsigned int states_before = g_state_calls;
        fire_button(BUTTON_SINGLE_CLICK);
        CHECK(g_event_count == 2 * cycle + 2);
        CHECK(g_state_calls == states_before);
        fire_button(BUTTON_LONG_PRESS_START);
        CHECK(g_provision_calls == provisions_before + 2);
    }

    g_state = MYBOT_STATE_READY;
    CHECK(ops->init(&context, blocking_event, &g_callback_entered) == 0);
    pthread_t click_worker;
    pthread_t destroy_worker;
    CHECK(pthread_create(&click_worker, NULL, click_thread, NULL) == 0);
    wait_for_callback_entry();
    atomic_store(&g_delay_calls, 0);
    atomic_store(&g_destroy_done, false);
    CHECK(pthread_create(&destroy_worker, NULL, destroy_thread, context) == 0);

    for (unsigned int elapsed_ms = 0; elapsed_ms < 2000 && !atomic_load(&g_delay_calls);
         ++elapsed_ms) {
        struct timespec delay = {.tv_nsec = 1000000L};
        nanosleep(&delay, NULL);
    }
    CHECK(atomic_load(&g_delay_calls) > 0);
    CHECK(!atomic_load(&g_destroy_done));

    pthread_mutex_lock(&g_callback_lock);
    g_callback_release = true;
    pthread_cond_broadcast(&g_callback_cond);
    pthread_mutex_unlock(&g_callback_lock);
    CHECK(pthread_join(click_worker, NULL) == 0);
    CHECK(pthread_join(destroy_worker, NULL) == 0);
    CHECK(atomic_load(&g_destroy_done));
    CHECK(ops->init(&context, record_event, &g_event_count) == 0);
    ops->destroy(context);
}

int main(void) {
    test_hardware_failures_and_retry();
    test_input_lifecycle();
    puts("AtomEchoS3R host fault-injection tests passed");
    return 0;
}
