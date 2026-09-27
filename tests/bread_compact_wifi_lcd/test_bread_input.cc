/* SPDX-License-Identifier: MIT */
#include "bread_mock_platform.h"

#include "board_config.h"

#include <mybot/mybot.h>
#include <mybot/platform/mybot_key.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);              \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (0)

struct mock_button {
    bool open;
    button_callback_t click;
    button_callback_t long_press;
    void *click_data;
    void *long_press_data;
};

static mock_button button;
static bool fail_constructor;
static bool partial_constructor;
static button_event_t failed_registration;
static unsigned int failed_deletes;
static unsigned int constructors;
static unsigned int registrations;
static unsigned int deletes;
static unsigned int state_calls;
static unsigned int provisioning_calls;
static unsigned int event_count;
static mybot_key_event_t last_event;
static mybot_state_t state;
static std::atomic_uint delay_calls;
static std::atomic_bool callback_returned;
static std::atomic_bool destroy_done;
static pthread_mutex_t callback_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t callback_condition = PTHREAD_COND_INITIALIZER;
static bool callback_entered;
static bool callback_release;

extern "C" {
int mybot_bread_input_start(void);
const mybot_key_ops_t *mybot_bread_input_ops(void);

const char *esp_err_to_name(esp_err_t error) {
    return error == ESP_OK ? "ESP_OK" : "ESP_FAIL";
}

esp_err_t iot_button_new_gpio_device(const button_config_t *config,
                                     const button_gpio_config_t *gpio_config,
                                     button_handle_t *out) {
    CHECK(config->long_press_time == 3000 && config->short_press_time == 50);
    CHECK(gpio_config->gpio_num == GPIO_NUM_0 && MYBOT_BOOT_BUTTON_GPIO == GPIO_NUM_0);
    CHECK(gpio_config->active_level == 0 && !gpio_config->enable_power_save);
    CHECK(!gpio_config->disable_pull && out && !button.open);
    ++constructors;
    if (fail_constructor) {
        fail_constructor = false;
        if (partial_constructor) {
            partial_constructor = false;
            button = {};
            button.open = true;
            *out = &button;
        }
        return ESP_FAIL;
    }
    button = {};
    button.open = true;
    *out = &button;
    return ESP_OK;
}

esp_err_t iot_button_register_cb(button_handle_t handle, button_event_t event, void *event_data,
                                 button_callback_t callback, void *user) {
    CHECK(handle == &button && button.open && !event_data && callback);
    ++registrations;
    if (failed_registration == event) {
        failed_registration = static_cast<button_event_t>(0);
        return ESP_FAIL;
    }
    if (event == BUTTON_SINGLE_CLICK) {
        button.click = callback;
        button.click_data = user;
    } else {
        CHECK(event == BUTTON_LONG_PRESS_START);
        button.long_press = callback;
        button.long_press_data = user;
    }
    return ESP_OK;
}

esp_err_t iot_button_delete(button_handle_t handle) {
    CHECK(handle == &button && button.open);
    ++deletes;
    if (failed_deletes > 0) {
        --failed_deletes;
        return ESP_FAIL;
    }
    button = {};
    return ESP_OK;
}

mybot_state_t mybot_get_state(void) {
    ++state_calls;
    return state;
}

int mybot_board_handle_boot_long_press(void) {
    ++provisioning_calls;
    return 0;
}

void vTaskDelay(unsigned int ticks) {
    ++delay_calls;
    const timespec delay = {.tv_sec = ticks / 1000, .tv_nsec = (ticks % 1000) * 1000000L};
    nanosleep(&delay, nullptr);
}
}

static void fire(button_event_t event) {
    CHECK(button.open);
    const button_callback_t callback =
        event == BUTTON_SINGLE_CLICK ? button.click : button.long_press;
    void *user = event == BUTTON_SINGLE_CLICK ? button.click_data : button.long_press_data;
    CHECK(callback);
    callback(&button, user);
}

static void record_event(mybot_key_event_t event, void *user) {
    CHECK(user == &event_count);
    ++event_count;
    last_event = event;
}

static void blocking_event(mybot_key_event_t event, void *user) {
    CHECK(event == MYBOT_KEY_EVENT_CONVERSATION_START && user == &callback_entered);
    pthread_mutex_lock(&callback_lock);
    callback_entered = true;
    pthread_cond_broadcast(&callback_condition);
    while (!callback_release) {
        pthread_cond_wait(&callback_condition, &callback_lock);
    }
    pthread_mutex_unlock(&callback_lock);
    callback_returned.store(true);
}

static void *click_thread(void *) {
    fire(BUTTON_SINGLE_CLICK);
    return nullptr;
}

static void *destroy_thread(void *context) {
    mybot_bread_input_ops()->destroy(context);
    CHECK(callback_returned.load());
    destroy_done.store(true);
    return nullptr;
}

static void test_startup_retry(const char *scenario) {
    const mybot_key_ops_t *ops = mybot_bread_input_ops();
    void *context = nullptr;
    CHECK(ops && ops->init(&context, record_event, &event_count) < 0 && !context);
    if (std::strcmp(scenario, "ok")) {
        if (!std::strcmp(scenario, "ctor")) {
            fail_constructor = true;
        } else if (!std::strcmp(scenario, "partial_ctor")) {
            fail_constructor = true;
            partial_constructor = true;
            failed_deletes = 2;
        } else {
            CHECK(!std::strcmp(scenario, "register_click") ||
                  !std::strcmp(scenario, "register_long"));
            failed_registration = !std::strcmp(scenario, "register_click")
                                      ? BUTTON_SINGLE_CLICK
                                      : BUTTON_LONG_PRESS_START;
            failed_deletes = 2;
        }
        CHECK(mybot_bread_input_start() < 0);
        CHECK(!fail_constructor && !partial_constructor && failed_registration == 0);
        CHECK(ops->init(&context, record_event, &event_count) < 0 && !context);
        if (std::strcmp(scenario, "ctor")) {
            CHECK(button.open && failed_deletes == 1);
            const unsigned int news = constructors;
            CHECK(mybot_bread_input_start() < 0);
            CHECK(button.open && failed_deletes == 0 && constructors == news);
            CHECK(ops->init(&context, record_event, &event_count) < 0 && !context);
        }
    }
    CHECK(mybot_bread_input_start() == 0 && button.open && button.click && button.long_press);
    const unsigned int news = constructors;
    const unsigned int registers = registrations;
    CHECK(mybot_bread_input_start() == 0 && mybot_bread_input_start() == 0);
    CHECK(constructors == news && registrations == registers);
}

static void test_attach_detach() {
    const mybot_key_ops_t *ops = mybot_bread_input_ops();
    void *context = nullptr;
    CHECK(ops->init(nullptr, record_event, &event_count) < 0);
    CHECK(ops->init(&context, nullptr, &event_count) < 0);
    ops->destroy(nullptr);
    for (unsigned int cycle = 0; cycle < 3; ++cycle) {
        CHECK(ops->init(&context, record_event, &event_count) == 0 && context);
        void *duplicate = nullptr;
        CHECK(ops->init(&duplicate, record_event, &event_count) < 0 && !duplicate);
        state = MYBOT_STATE_READY;
        fire(BUTTON_SINGLE_CLICK);
        CHECK(last_event == MYBOT_KEY_EVENT_CONVERSATION_START && event_count == 2 * cycle + 1);
        state = MYBOT_STATE_IN_CONVERSATION;
        fire(BUTTON_SINGLE_CLICK);
        CHECK(last_event == MYBOT_KEY_EVENT_CONVERSATION_STOP && event_count == 2 * cycle + 2);
        state = MYBOT_STATE_STARTING_SERVICES;
        fire(BUTTON_SINGLE_CLICK);
        CHECK(event_count == 2 * cycle + 2);
        const unsigned int provisions = provisioning_calls;
        fire(BUTTON_LONG_PRESS_START);
        CHECK(provisioning_calls == provisions + 1);
        const unsigned int freed = deletes;
        ops->destroy(context);
        CHECK(button.open && deletes == freed);
        const unsigned int states = state_calls;
        fire(BUTTON_SINGLE_CLICK);
        CHECK(event_count == 2 * cycle + 2 && state_calls == states);
        fire(BUTTON_LONG_PRESS_START);
        CHECK(provisioning_calls == provisions + 2);
        ops->destroy(context);
    }
}

static void test_callback_drain() {
    const mybot_key_ops_t *ops = mybot_bread_input_ops();
    void *context = nullptr;
    CHECK(ops->init(&context, blocking_event, &callback_entered) == 0);
    state = MYBOT_STATE_READY;
    pthread_t click_worker;
    pthread_t destroy_worker;
    CHECK(pthread_create(&click_worker, nullptr, click_thread, nullptr) == 0);
    timespec deadline{};
    CHECK(clock_gettime(CLOCK_REALTIME, &deadline) == 0);
    deadline.tv_sec += 2;
    pthread_mutex_lock(&callback_lock);
    while (!callback_entered) {
        const int result = pthread_cond_timedwait(&callback_condition, &callback_lock, &deadline);
        CHECK(result == 0 && result != ETIMEDOUT);
    }
    pthread_mutex_unlock(&callback_lock);
    delay_calls.store(0);
    CHECK(pthread_create(&destroy_worker, nullptr, destroy_thread, context) == 0);
    for (unsigned int elapsed = 0; elapsed < 2000 && delay_calls.load() == 0; ++elapsed) {
        const timespec pause = {.tv_sec = 0, .tv_nsec = 1000000L};
        nanosleep(&pause, nullptr);
    }
    CHECK(delay_calls.load() > 0 && !destroy_done.load() && !callback_returned.load());
    void *premature = nullptr;
    CHECK(ops->init(&premature, record_event, &event_count) < 0 && !premature);
    const unsigned int count_during_detach = event_count;
    fire(BUTTON_SINGLE_CLICK);
    CHECK(event_count == count_during_detach);
    const unsigned int provisions = provisioning_calls;
    fire(BUTTON_LONG_PRESS_START);
    CHECK(provisioning_calls == provisions + 1);
    pthread_mutex_lock(&callback_lock);
    callback_release = true;
    pthread_cond_broadcast(&callback_condition);
    pthread_mutex_unlock(&callback_lock);
    CHECK(pthread_join(click_worker, nullptr) == 0 && pthread_join(destroy_worker, nullptr) == 0);
    CHECK(callback_returned.load() && destroy_done.load());
    CHECK(ops->init(&context, record_event, &event_count) == 0);
    const unsigned int count = event_count;
    fire(BUTTON_SINGLE_CLICK);
    CHECK(event_count == count + 1);
    ops->destroy(context);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    CHECK(MYBOT_DISPLAY_WIDTH == 240 && MYBOT_DISPLAY_HEIGHT == 320);
    test_startup_retry(argv[1]);
    test_attach_detach();
    test_callback_drain();
    std::printf("Bread single-button host fault-injection scenario passed: %s\n", argv[1]);
    return 0;
}
