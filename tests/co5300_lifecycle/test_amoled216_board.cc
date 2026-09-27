/* SPDX-License-Identifier: MIT */
#include "host_platform.h"

#include "amoled175_hardware.h"
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

static bool hardware_open;
static bool hardware_ready;
static bool touch_io_open;
static bool input_running;
static unsigned int lcd_references;
static unsigned int hardware_initializations;
static unsigned int hardware_deinitializations;
static unsigned int lcd_initializations;
static unsigned int lcd_destroys;
static unsigned int input_stops;
static unsigned int failed_input_stops;
static int lcd_context;
static std::string failure;
static std::string cleanup_failure;
static std::vector<std::string> events;

static int step(const char *name) {
    events.emplace_back(name);
    if (failure == name) {
        failure.clear();
        return -1;
    }
    if (cleanup_failure == name) {
        cleanup_failure.clear();
        return -1;
    }
    return 0;
}

static size_t event_position(const char *name) {
    for (size_t i = 0; i < events.size(); ++i) {
        if (events[i] == name) {
            return i;
        }
    }
    CHECK(false);
    return 0;
}

static int lcd_init(void **out) {
    CHECK(out && hardware_ready);
    ++lcd_initializations;
    *out = nullptr;
    if (step("lcd_init") < 0) {
        return -1;
    }
    ++lcd_references;
    *out = &lcd_context;
    return 0;
}

static int lcd_render(void *context, const mybot_lcd_content_t *content) {
    CHECK(context == &lcd_context && lcd_references > 0 && content);
    CHECK(content->screen == MYBOT_LCD_SCREEN_STARTING ||
          content->screen == MYBOT_LCD_SCREEN_WIFI_PROVISIONING);
    return step("lcd_render");
}

static void lcd_destroy(void *context) {
    CHECK(context == &lcd_context && lcd_references > 0);
    CHECK(!touch_io_open && !input_running);
    events.emplace_back("lcd_destroy");
    ++lcd_destroys;
    --lcd_references;
}

static const mybot_lcd_ops_t lcd_ops = {lcd_init, lcd_render, lcd_destroy};

extern "C" {
const char *esp_err_to_name(esp_err_t error) {
    return error == ESP_OK ? "ESP_OK" : "ESP_FAIL";
}

int mybot_amoled175_hardware_init(void) {
    ++hardware_initializations;
    /* Pending child handles must be released before initialization can retry. */
    CHECK(!hardware_open || hardware_ready);
    if (step("hardware_init") < 0) {
        return -1;
    }
    hardware_open = true;
    hardware_ready = true;
    return 0;
}

int mybot_amoled175_hardware_deinit(void) {
    CHECK(hardware_open && !touch_io_open && !input_running && lcd_references == 0);
    ++hardware_deinitializations;
    hardware_ready = false;
    if (step("hardware_deinit") < 0) {
        return -1;
    }
    hardware_open = false;
    return 0;
}

int mybot_amoled175_input_start(void) {
    CHECK(hardware_ready && lcd_references > 0 && !touch_io_open && !input_running);
    touch_io_open = true;
    if (step("input_start") < 0) {
        return -1;
    }
    input_running = true;
    return 0;
}

int mybot_amoled175_input_stop(void) {
    ++input_stops;
    CHECK(step("input_stop") == 0);
    input_running = false;
    if (failed_input_stops > 0) {
        --failed_input_stops;
        return -1;
    }
    touch_io_open = false;
    return 0;
}

const mybot_lcd_ops_t *mybot_shared_lvgl_ops(void) {
    return &lcd_ops;
}

#define MOCK_OPS(name, type)                                                                       \
    const type *name(void) {                                                                       \
        static const type ops{};                                                                   \
        return &ops;                                                                               \
    }
MOCK_OPS(mybot_amoled175_audio_capture_ops, mybot_audio_capture_ops_t)
MOCK_OPS(mybot_amoled175_audio_playback_ops, mybot_audio_playback_ops_t)
MOCK_OPS(mybot_amoled175_audio_volume_ops, mybot_audio_volume_ops_t)
MOCK_OPS(mybot_esp32s3_announce_ops, mybot_announce_ops_t)
MOCK_OPS(mybot_amoled175_input_ops, mybot_key_ops_t)
MOCK_OPS(mybot_esp32s3_https_ops, mybot_https_ops_t)
MOCK_OPS(mybot_esp32s3_kv_store_ops, mybot_kv_store_ops_t)
MOCK_OPS(mybot_esp32s3_wifi_ops, mybot_wifi_ops_t)

int mybot_platform_register(const mybot_platform_descriptor_t *descriptor) {
    CHECK(descriptor && descriptor->lcd == &lcd_ops && input_running && hardware_ready);
    CHECK(descriptor->key && descriptor->audio_capture && descriptor->audio_playback);
    CHECK(descriptor->wifi && descriptor->kv_store && descriptor->audio_volume &&
          descriptor->https);
    CHECK(descriptor->announce && !descriptor->video);
    return 0;
}
void mybot_board_request_wifi_provisioning(void) {
}
bool mybot_board_wait_wifi_provisioning_request(uint32_t timeout_ms) {
    CHECK(timeout_ms == 0);
    return false;
}
int mybot_wifi_ensure_network(const char *device_id,
                              mybot_wifi_provisioning_handler_t on_provisioning) {
    CHECK(device_id && on_provisioning);
    return -17;
}
int mybot_wifi_run_provisioning(mybot_wifi_provisioning_handler_t on_provisioning) {
    CHECK(on_provisioning);
    return -23;
}
void mybot_wifi_shutdown_network(void) {
}
int mybot_embedded_ogg_play_wifi_provisioning(const mybot_audio_playback_ops_t *playback,
                                              const mybot_audio_volume_ops_t *volume,
                                              const char *trigger) {
    CHECK(playback && volume && trigger);
    return 0;
}
}

static void assert_prepared(const mybot_board_t *board) {
    CHECK(hardware_open && hardware_ready && touch_io_open && input_running && lcd_references == 1);
    const unsigned int inits = hardware_initializations;
    const unsigned int lcd_inits = lcd_initializations;
    const unsigned int stops = input_stops;
    const size_t calls = events.size();
    CHECK(board->prepare() == 0 && board->prepare() == 0);
    CHECK(hardware_initializations == inits && lcd_initializations == lcd_inits &&
          input_stops == stops);
    CHECK(events.size() == calls && lcd_references == 1 && input_running);
    CHECK(board->register_platform() == 0);
    CHECK(board->ensure_network("host-test") == -17);
    CHECK(board->provision_wifi() == -23);
}

static void test_initialization_failure(const mybot_board_t *board, const char *scenario) {
    failure = scenario;
    CHECK(board->prepare() < 0 && failure.empty());
    CHECK(!hardware_open && !hardware_ready && !touch_io_open && !input_running &&
          lcd_references == 0);
    if (!std::strcmp(scenario, "hardware_init")) {
        CHECK(input_stops == 0 && hardware_deinitializations == 0);
    } else {
        CHECK(event_position("input_stop") < event_position("hardware_deinit"));
        if (std::strcmp(scenario, "lcd_init")) {
            CHECK(event_position("input_stop") < event_position("lcd_destroy"));
            CHECK(event_position("lcd_destroy") < event_position("hardware_deinit"));
            CHECK(lcd_destroys == 1);
        }
    }
    CHECK(board->prepare() == 0);
    assert_prepared(board);
}

static void test_input_cleanup_retry(const mybot_board_t *board) {
    failure = "input_start";
    failed_input_stops = 2;
    CHECK(board->prepare() < 0 && failure.empty());
    CHECK(touch_io_open && hardware_ready && lcd_references == 1);
    CHECK(input_stops == 1 && lcd_destroys == 0 && hardware_deinitializations == 0);
    CHECK(board->prepare() < 0);
    CHECK(input_stops == 2 && hardware_initializations == 1 && lcd_initializations == 1);
    CHECK(touch_io_open && hardware_ready && lcd_references == 1);
    CHECK(lcd_destroys == 0 && hardware_deinitializations == 0);
    events.clear();
    CHECK(board->prepare() == 0);
    CHECK(input_stops == 3 && hardware_deinitializations == 1 && lcd_destroys == 1);
    CHECK(hardware_initializations == 2 && lcd_initializations == 2);
    CHECK(event_position("input_stop") < event_position("lcd_destroy"));
    CHECK(event_position("lcd_destroy") < event_position("hardware_deinit"));
    CHECK(event_position("hardware_deinit") < event_position("hardware_init"));
    assert_prepared(board);
}

static void test_hardware_cleanup_retry(const mybot_board_t *board) {
    failure = "lcd_render";
    cleanup_failure = "hardware_deinit";
    CHECK(board->prepare() < 0 && failure.empty() && cleanup_failure.empty());
    CHECK(hardware_open && !hardware_ready && !touch_io_open && lcd_references == 0);
    CHECK(lcd_destroys == 1 && hardware_deinitializations == 1);
    events.clear();
    CHECK(board->prepare() == 0);
    CHECK(lcd_destroys == 1 && hardware_deinitializations == 2);
    CHECK(event_position("hardware_deinit") < event_position("hardware_init"));
    assert_prepared(board);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    const mybot_board_t *board = mybot_board_get();
    CHECK(board && !std::strcmp(board->id, "esp32-s3-touch-amoled-2.16"));
    if (!std::strcmp(argv[1], "input_stop_repeat")) {
        test_input_cleanup_retry(board);
    } else if (!std::strcmp(argv[1], "hardware_deinit")) {
        test_hardware_cleanup_retry(board);
    } else if (!std::strcmp(argv[1], "ok")) {
        CHECK(board->prepare() == 0);
        assert_prepared(board);
    } else {
        test_initialization_failure(board, argv[1]);
    }
    std::printf("AMOLED 2.16 board recovery host scenario passed: %s\n", argv[1]);
    return 0;
}
