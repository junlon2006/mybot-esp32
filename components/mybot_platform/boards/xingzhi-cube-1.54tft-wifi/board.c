/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_platform/board.h"

#include <mybot/platform/mybot_platform.h>

#include "announcement/embedded_ogg_prompt.h"
#include "board_config.h"
#include "display/board_backlight.h"
#include "network/wifi_control.h"
#include "platform/board_actions.h"

#include "driver/ledc.h"
#include "driver/rtc_io.h"
#include "esp_log.h"

#include <stdbool.h>

#define TAG "xingzhi_board"
#define BACKLIGHT_MAX_DUTY ((1U << 13) - 1U)

const mybot_audio_capture_ops_t *mybot_esp32s3_audio_capture_ops(void);
const mybot_audio_playback_ops_t *mybot_esp32s3_audio_playback_ops(void);
const mybot_audio_volume_ops_t *mybot_esp32s3_audio_volume_ops(void);
const mybot_announce_ops_t *mybot_esp32s3_announce_ops(void);
const mybot_key_ops_t *mybot_esp32s3_button_ops(void);
const mybot_https_ops_t *mybot_esp32s3_https_ops(void);
const mybot_kv_store_ops_t *mybot_esp32s3_kv_store_ops(void);
const mybot_lcd_ops_t *mybot_shared_lvgl_ops(void);
const mybot_wifi_ops_t *mybot_esp32s3_wifi_ops(void);
int mybot_esp32s3_buttons_start(void);

static const mybot_lcd_ops_t *s_lcd_ops;
static void *s_lcd_ctx;
static bool s_prepared;
static bool s_backlight_ready;
static bool s_automatic_provisioning_started;

int mybot_board_set_display_backlight(unsigned percent) {
    if (percent > 100 || !s_backlight_ready) {
        return -1;
    }
    uint32_t duty = (BACKLIGHT_MAX_DUTY * percent + 50U) / 100U;
    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty) != ESP_OK ||
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0) != ESP_OK) {
        return -1;
    }
    return 0;
}

static int prepare_hardware(void) {
    /* Release a previous deep-sleep hold, then keep the system rail on for this process. */
    const gpio_config_t power = {
        .pin_bit_mask = 1ULL << MYBOT_BOARD_POWER_HOLD,
        .mode = GPIO_MODE_OUTPUT,
    };
    if (rtc_gpio_hold_dis(MYBOT_BOARD_POWER_HOLD) != ESP_OK ||
        rtc_gpio_deinit(MYBOT_BOARD_POWER_HOLD) != ESP_OK ||
        gpio_set_level(MYBOT_BOARD_POWER_HOLD, 1) != ESP_OK || gpio_config(&power) != ESP_OK) {
        return -1;
    }
    if (s_backlight_ready) {
        return mybot_board_set_display_backlight(0);
    }
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    const ledc_channel_config_t channel = {
        .gpio_num = MYBOT_DISPLAY_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
    };
    if (ledc_timer_config(&timer) != ESP_OK || ledc_channel_config(&channel) != ESP_OK) {
        return -1;
    }
    s_backlight_ready = true;
    return 0;
}

static int board_show_screen(mybot_lcd_screen_t screen) {
    if (!s_lcd_ops || !s_lcd_ctx) {
        return -1;
    }
    const mybot_lcd_content_t content = {.screen = screen};
    return s_lcd_ops->render(s_lcd_ctx, &content);
}

static int board_prepare(void) {
    if (s_prepared) {
        return 0;
    }
    if (prepare_hardware() < 0) {
        ESP_LOGE(TAG, "event=board_prepare component=power_backlight result=error");
        return -1;
    }
    s_lcd_ops = mybot_shared_lvgl_ops();
    if (!s_lcd_ops || s_lcd_ops->init(&s_lcd_ctx) < 0) {
        ESP_LOGE(TAG, "event=board_prepare component=display result=error");
        return -1;
    }
    if (board_show_screen(MYBOT_LCD_SCREEN_STARTING) < 0 || mybot_esp32s3_buttons_start() < 0) {
        s_lcd_ops->destroy(s_lcd_ctx);
        s_lcd_ctx = NULL;
        ESP_LOGE(TAG, "event=board_prepare component=display_buttons result=error");
        return -1;
    }
    s_prepared = true;
    return 0;
}

int mybot_board_handle_boot_long_press(void) {
    mybot_board_request_wifi_provisioning();
    return 0;
}

static void board_on_provisioning(const char *trigger) {
    if (board_show_screen(MYBOT_LCD_SCREEN_WIFI_PROVISIONING) < 0) {
        ESP_LOGW(TAG, "event=display screen=wifi_provisioning result=error");
    }
    (void)mybot_embedded_ogg_play_wifi_provisioning(mybot_esp32s3_audio_playback_ops(),
                                                    mybot_esp32s3_audio_volume_ops(), trigger);
}

static void board_on_automatic_provisioning(void) {
    s_automatic_provisioning_started = true;
    board_on_provisioning("automatic");
}

static void board_on_button_provisioning(void) {
    board_on_provisioning("button");
}

static int board_ensure_network(const char *device_id) {
    s_automatic_provisioning_started = false;
    int result = mybot_wifi_ensure_network(device_id, board_on_automatic_provisioning);
    if (s_automatic_provisioning_started) {
        (void)mybot_board_wait_wifi_provisioning_request(0);
    }
    return result;
}

static int board_provision_wifi(void) {
    return mybot_wifi_run_provisioning(board_on_button_provisioning);
}

static void board_shutdown_network(void) {
    mybot_wifi_shutdown_network();
}

static int board_register_platform(void) {
    const mybot_platform_descriptor_t descriptor = {
        .wifi = mybot_esp32s3_wifi_ops(),
        .kv_store = mybot_esp32s3_kv_store_ops(),
        .key = mybot_esp32s3_button_ops(),
        .audio_capture = mybot_esp32s3_audio_capture_ops(),
        .audio_playback = mybot_esp32s3_audio_playback_ops(),
        .audio_volume = mybot_esp32s3_audio_volume_ops(),
        .announce = mybot_esp32s3_announce_ops(),
        .https = mybot_esp32s3_https_ops(),
        .lcd = mybot_shared_lvgl_ops(),
    };
    return mybot_platform_register(&descriptor);
}

static const mybot_board_t s_board = {
    .id = MYBOT_BOARD_NAME,
    .hw_model = MYBOT_BOARD_NAME,
    .prepare = board_prepare,
    .register_platform = board_register_platform,
    .ensure_network = board_ensure_network,
    .provision_wifi = board_provision_wifi,
    .shutdown_network = board_shutdown_network,
};

const mybot_board_t *mybot_board_get(void) {
    return &s_board;
}
