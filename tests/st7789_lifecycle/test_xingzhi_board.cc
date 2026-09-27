/* SPDX-License-Identifier: MIT */
#include "host_platform.h"

#include "board_config.h"
#include "mybot_platform/board.h"
#include "network/wifi_control.h"
#include "platform/board_actions.h"

#include <mybot/platform/mybot_platform.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);              \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (0)

static bool hold_disabled;
static bool rtc_deinitialized;
static bool power_configured;
static bool power_high;
static bool timer_configured;
static bool backlight_configured;
static bool lcd_open;
static bool buttons_started;
static uint32_t backlight_duty;
static uint32_t max_duty;
static unsigned int lcd_initializations;
static unsigned int lcd_destroys;
static unsigned int button_starts;
static unsigned int provisioning_requests;
static unsigned int waits;
static unsigned int announcements;
static unsigned int shutdowns;
static bool automatic_provisioning;
static int lcd_context;
static std::string failed_step;
static std::vector<std::string> events;

static esp_err_t step(const char *name) {
    events.emplace_back(name);
    if (failed_step == name) {
        failed_step.clear();
        return ESP_FAIL;
    }
    return ESP_OK;
}

static int lcd_init(void **out) {
    CHECK(out && power_configured && power_high && backlight_configured && backlight_duty == 0 &&
          !lcd_open);
    ++lcd_initializations;
    *out = nullptr;
    if (step("lcd_init") != ESP_OK || mybot_board_set_display_backlight(0) < 0) {
        return -1;
    }
    lcd_open = true;
    *out = &lcd_context;
    return 0;
}

static int lcd_render(void *context, const mybot_lcd_content_t *content) {
    CHECK(context == &lcd_context && lcd_open && content);
    CHECK(content->screen == MYBOT_LCD_SCREEN_STARTING ||
          content->screen == MYBOT_LCD_SCREEN_WIFI_PROVISIONING);
    return step("lcd_render") == ESP_OK ? 0 : -1;
}

static void lcd_destroy(void *context) {
    CHECK(context == &lcd_context && lcd_open);
    events.emplace_back("lcd_destroy");
    ++lcd_destroys;
    lcd_open = false;
}

static const mybot_lcd_ops_t lcd_ops = {lcd_init, lcd_render, lcd_destroy};

extern "C" {
const char *esp_err_to_name(esp_err_t error) {
    return error == ESP_OK ? "ESP_OK" : "ESP_FAIL";
}
esp_err_t rtc_gpio_hold_dis(gpio_num_t gpio) {
    CHECK(gpio == MYBOT_BOARD_POWER_HOLD);
    const esp_err_t result = step("rtc_hold_dis");
    if (result == ESP_OK) {
        hold_disabled = true;
    }
    return result;
}
esp_err_t rtc_gpio_deinit(gpio_num_t gpio) {
    CHECK(gpio == MYBOT_BOARD_POWER_HOLD && hold_disabled);
    const esp_err_t result = step("rtc_deinit");
    if (result == ESP_OK) {
        rtc_deinitialized = true;
    }
    return result;
}
esp_err_t gpio_config(const gpio_config_t *config) {
    CHECK(config->pin_bit_mask == (1ULL << MYBOT_BOARD_POWER_HOLD));
    CHECK(config->mode == GPIO_MODE_OUTPUT && rtc_deinitialized && power_high);
    const esp_err_t result = step("gpio_config");
    if (result == ESP_OK) {
        power_configured = true;
    }
    return result;
}
esp_err_t gpio_set_level(gpio_num_t gpio, uint32_t level) {
    CHECK(gpio == MYBOT_BOARD_POWER_HOLD && level == 1 && rtc_deinitialized);
    const esp_err_t result = step("gpio_high");
    if (result == ESP_OK) {
        power_high = true;
    }
    return result;
}
esp_err_t ledc_timer_config(const ledc_timer_config_t *config) {
    CHECK(power_configured && power_high && config->freq_hz == 5000 &&
          config->speed_mode == LEDC_LOW_SPEED_MODE &&
          config->duty_resolution == LEDC_TIMER_13_BIT);
    const esp_err_t result = step("ledc_timer");
    if (result == ESP_OK) {
        timer_configured = true;
        max_duty = (1u << config->duty_resolution) - 1;
    }
    return result;
}
esp_err_t ledc_channel_config(const ledc_channel_config_t *config) {
    CHECK(timer_configured && config->gpio_num == MYBOT_DISPLAY_BACKLIGHT && config->duty == 0);
    const esp_err_t result = step("ledc_channel");
    if (result == ESP_OK) {
        backlight_configured = true;
        backlight_duty = 0;
    }
    return result;
}
esp_err_t ledc_set_duty(int speed_mode, int channel, uint32_t duty) {
    CHECK(speed_mode == LEDC_LOW_SPEED_MODE && channel == LEDC_CHANNEL_0);
    CHECK(backlight_configured && duty <= max_duty);
    const esp_err_t result = step("ledc_duty");
    if (result == ESP_OK) {
        backlight_duty = duty;
    }
    return result;
}
esp_err_t ledc_update_duty(int speed_mode, int channel) {
    CHECK(speed_mode == LEDC_LOW_SPEED_MODE && channel == LEDC_CHANNEL_0 && backlight_configured);
    return step("ledc_update");
}
const mybot_lcd_ops_t *mybot_shared_lvgl_ops(void) {
    return &lcd_ops;
}
int mybot_esp32s3_buttons_start(void) {
    CHECK(lcd_open && !buttons_started);
    ++button_starts;
    if (step("buttons_start") != ESP_OK) {
        return -1;
    }
    buttons_started = true;
    return 0;
}
#define MOCK_OPS(name, type)                                                                       \
    const type *name(void) {                                                                       \
        static const type ops{};                                                                   \
        return &ops;                                                                               \
    }
MOCK_OPS(mybot_esp32s3_audio_capture_ops, mybot_audio_capture_ops_t)
MOCK_OPS(mybot_esp32s3_audio_playback_ops, mybot_audio_playback_ops_t)
MOCK_OPS(mybot_esp32s3_audio_volume_ops, mybot_audio_volume_ops_t)
MOCK_OPS(mybot_esp32s3_announce_ops, mybot_announce_ops_t)
MOCK_OPS(mybot_esp32s3_button_ops, mybot_key_ops_t)
MOCK_OPS(mybot_esp32s3_https_ops, mybot_https_ops_t)
MOCK_OPS(mybot_esp32s3_kv_store_ops, mybot_kv_store_ops_t)
MOCK_OPS(mybot_esp32s3_wifi_ops, mybot_wifi_ops_t)

int mybot_platform_register(const mybot_platform_descriptor_t *descriptor) {
    CHECK(power_high && lcd_open && buttons_started);
    CHECK(descriptor && descriptor->lcd == &lcd_ops);
    CHECK(descriptor->wifi && descriptor->kv_store && descriptor->key && descriptor->audio_capture);
    CHECK(descriptor->audio_playback && descriptor->audio_volume && descriptor->announce);
    CHECK(descriptor->https && !descriptor->video);
    return 0;
}
void mybot_board_request_wifi_provisioning(void) {
    ++provisioning_requests;
}
bool mybot_board_wait_wifi_provisioning_request(uint32_t timeout_ms) {
    CHECK(timeout_ms == 0);
    ++waits;
    return false;
}
int mybot_wifi_ensure_network(const char *device_id,
                              mybot_wifi_provisioning_handler_t on_provisioning) {
    CHECK(device_id && !std::strcmp(device_id, "host-test") && on_provisioning);
    if (automatic_provisioning) {
        on_provisioning();
    }
    return 17;
}
int mybot_wifi_run_provisioning(mybot_wifi_provisioning_handler_t on_provisioning) {
    CHECK(on_provisioning);
    on_provisioning();
    return 23;
}
void mybot_wifi_shutdown_network(void) {
    ++shutdowns;
}
int mybot_embedded_ogg_play_wifi_provisioning(const mybot_audio_playback_ops_t *playback,
                                              const mybot_audio_volume_ops_t *volume,
                                              const char *trigger) {
    CHECK(playback && volume && trigger);
    CHECK(!std::strcmp(trigger, "automatic") || !std::strcmp(trigger, "button"));
    ++announcements;
    return 0;
}
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    const mybot_board_t *board = mybot_board_get();
    CHECK(board && !std::strcmp(board->id, "xingzhi-cube-1.54tft-wifi"));
    CHECK(mybot_board_set_display_backlight(100) < 0);
    CHECK(mybot_board_set_display_backlight(101) < 0);
    if (std::strcmp(argv[1], "ok")) {
        failed_step = argv[1];
        CHECK(board->prepare() < 0);
        CHECK(failed_step.empty() && !lcd_open && !buttons_started);
        if (!std::strcmp(argv[1], "lcd_render") || !std::strcmp(argv[1], "buttons_start")) {
            CHECK(lcd_destroys == 1 && power_high && backlight_configured);
        }
    }
    CHECK(board->prepare() == 0);
    CHECK(power_high && lcd_open && buttons_started && backlight_configured);
    const unsigned int lcd_inits = lcd_initializations;
    const unsigned int starts = button_starts;
    const size_t hardware_calls = events.size();
    CHECK(board->prepare() == 0 && board->prepare() == 0);
    CHECK(lcd_initializations == lcd_inits && button_starts == starts &&
          events.size() == hardware_calls);
    CHECK(mybot_board_set_display_backlight(100) == 0 && backlight_duty == max_duty);
    CHECK(mybot_board_set_display_backlight(0) == 0 && backlight_duty == 0);
    CHECK(board->register_platform() == 0);
    CHECK(mybot_board_handle_boot_long_press() == 0 && provisioning_requests == 1);
    CHECK(board->ensure_network("host-test") == 17 && waits == 0 && announcements == 0);
    automatic_provisioning = true;
    CHECK(board->ensure_network("host-test") == 17 && waits == 1 && announcements == 1);
    CHECK(board->provision_wifi() == 23 && announcements == 2);
    board->shutdown_network();
    CHECK(shutdowns == 1 && power_high && lcd_open);
    std::printf("Xingzhi board host scenario passed: %s\n", argv[1]);
    return 0;
}
