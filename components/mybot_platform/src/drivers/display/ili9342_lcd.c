/* SPDX-License-Identifier: Apache-2.0 */
#include "board_config.h"

#include <mybot/platform/mybot_lcd.h>

#include "cores3_hardware.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define TAG "cores3_lcd"
#define LCD_TRANSFER_ROWS 16
#define LCD_TRANSFER_TIMEOUT_MS 1000

#define RGB565(red, green, blue)                                                                   \
    (uint16_t)((((red) & 0xf8) << 8) | (((green) & 0xfc) << 3) | ((blue) >> 3))

typedef struct {
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
    SemaphoreHandle_t transfer_done;
    bool spi_ready;
    bool initialized;
    bool transfer_failed;
    unsigned int users;
} lcd_context_t;

static lcd_context_t s_context;
static DMA_ATTR uint16_t s_draw_buffer[MYBOT_DISPLAY_WIDTH * LCD_TRANSFER_ROWS];

static const uint8_t s_digits[10][5] = {
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, {0x00, 0x42, 0x7f, 0x40, 0x00}, {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4b, 0x31}, {0x18, 0x14, 0x12, 0x7f, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03}, {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1e},
};

static const uint8_t s_letters[26][5] = {
    {0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36}, {0x3e, 0x41, 0x41, 0x41, 0x22},
    {0x7f, 0x41, 0x41, 0x22, 0x1c}, {0x7f, 0x49, 0x49, 0x49, 0x41}, {0x7f, 0x09, 0x09, 0x09, 0x01},
    {0x3e, 0x41, 0x49, 0x49, 0x7a}, {0x7f, 0x08, 0x08, 0x08, 0x7f}, {0x00, 0x41, 0x7f, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3f, 0x01}, {0x7f, 0x08, 0x14, 0x22, 0x41}, {0x7f, 0x40, 0x40, 0x40, 0x40},
    {0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f}, {0x3e, 0x41, 0x41, 0x41, 0x3e},
    {0x7f, 0x09, 0x09, 0x09, 0x06}, {0x3e, 0x41, 0x51, 0x21, 0x5e}, {0x7f, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7f, 0x01, 0x01}, {0x3f, 0x40, 0x40, 0x40, 0x3f},
    {0x1f, 0x20, 0x40, 0x20, 0x1f}, {0x3f, 0x40, 0x38, 0x40, 0x3f}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
};

static bool on_color_transfer_done(esp_lcd_panel_io_handle_t panel_io,
                                   esp_lcd_panel_io_event_data_t *event_data, void *user_data) {
    (void)panel_io;
    (void)event_data;
    lcd_context_t *ctx = user_data;
    BaseType_t high_priority_woken = pdFALSE;
    xSemaphoreGiveFromISR(ctx->transfer_done, &high_priority_woken);
    return high_priority_woken == pdTRUE;
}

static const uint8_t *glyph_for(char character) {
    static const uint8_t space[5] = {0};
    static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
    if (character >= '0' && character <= '9') {
        return s_digits[character - '0'];
    }
    if (character >= 'A' && character <= 'Z') {
        return s_letters[character - 'A'];
    }
    return character == '-' ? dash : space;
}

static int submit_bitmap(int x, int y, int width, int height) {
    if (s_context.transfer_failed) {
        return -1;
    }
    while (xSemaphoreTake(s_context.transfer_done, 0) == pdTRUE) {
    }
    if (esp_lcd_panel_draw_bitmap(s_context.panel, x, y, x + width, y + height, s_draw_buffer) !=
        ESP_OK) {
        return -1;
    }
    if (xSemaphoreTake(s_context.transfer_done, pdMS_TO_TICKS(LCD_TRANSFER_TIMEOUT_MS)) != pdTRUE) {
        s_context.transfer_failed = true;
        ESP_LOGE(TAG, "event=lcd action=transfer result=error reason=timeout");
        return -1;
    }
    return 0;
}

static int fill_screen(uint16_t color) {
    for (size_t i = 0; i < sizeof(s_draw_buffer) / sizeof(s_draw_buffer[0]); ++i) {
        s_draw_buffer[i] = color;
    }
    for (int y = 0; y < MYBOT_DISPLAY_HEIGHT; y += LCD_TRANSFER_ROWS) {
        int rows = MYBOT_DISPLAY_HEIGHT - y;
        if (rows > LCD_TRANSFER_ROWS) {
            rows = LCD_TRANSFER_ROWS;
        }
        if (submit_bitmap(0, y, MYBOT_DISPLAY_WIDTH, rows) < 0) {
            return -1;
        }
    }
    return 0;
}

static int draw_character(int x, int y, char character, int scale, uint16_t foreground,
                          uint16_t background) {
    const int width = 6 * scale;
    const int height = 7 * scale;
    const uint8_t *glyph = glyph_for(character);

    for (int pixel_y = 0; pixel_y < height; ++pixel_y) {
        const int glyph_y = pixel_y / scale;
        for (int pixel_x = 0; pixel_x < width; ++pixel_x) {
            const int glyph_x = pixel_x / scale;
            const bool enabled = glyph_x < 5 && (glyph[glyph_x] & (1U << glyph_y)) != 0;
            s_draw_buffer[pixel_y * width + pixel_x] = enabled ? foreground : background;
        }
    }
    return submit_bitmap(x, y, width, height);
}

static int draw_text_centered(int y, const char *text, int scale, uint16_t foreground,
                              uint16_t background) {
    const size_t length = strlen(text);
    const int width = (int)length * 6 * scale;
    const int x = (MYBOT_DISPLAY_WIDTH - width) / 2;
    if (x < 0) {
        return -1;
    }
    for (size_t i = 0; i < length; ++i) {
        if (draw_character(x + (int)i * 6 * scale, y, text[i], scale, foreground, background) < 0) {
            return -1;
        }
    }
    return 0;
}

static void release_lcd(void) {
    if (s_context.panel) {
        esp_lcd_panel_del(s_context.panel);
        s_context.panel = NULL;
    }
    if (s_context.io) {
        esp_lcd_panel_io_del(s_context.io);
        s_context.io = NULL;
    }
    if (s_context.spi_ready) {
        spi_bus_free(SPI3_HOST);
        s_context.spi_ready = false;
    }
    if (s_context.transfer_done) {
        vSemaphoreDelete(s_context.transfer_done);
        s_context.transfer_done = NULL;
    }
}

static int lcd_init(void **out_ctx) {
    if (!out_ctx || !mybot_cores3_i2c_bus_handle()) {
        return -1;
    }
    *out_ctx = NULL;
    if (s_context.initialized) {
        ++s_context.users;
        *out_ctx = &s_context;
        ESP_LOGI(TAG, "event=lcd action=attach references=%u", s_context.users);
        return 0;
    }

    s_context.transfer_done = xSemaphoreCreateBinary();
    if (!s_context.transfer_done) {
        return -1;
    }
    const spi_bus_config_t bus_config = {
        .mosi_io_num = MYBOT_DISPLAY_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = MYBOT_DISPLAY_SCLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = sizeof(s_draw_buffer),
    };
    if (spi_bus_initialize(SPI3_HOST, &bus_config, SPI_DMA_CH_AUTO) != ESP_OK) {
        release_lcd();
        return -1;
    }
    s_context.spi_ready = true;

    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = MYBOT_DISPLAY_CS,
        .dc_gpio_num = MYBOT_DISPLAY_DC,
        .spi_mode = 2,
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 1,
        .on_color_trans_done = on_color_transfer_done,
        .user_ctx = &s_context,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    if (esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &s_context.io) != ESP_OK) {
        release_lcd();
        return -1;
    }

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_NC,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    if (esp_lcd_new_panel_ili9341(s_context.io, &panel_config, &s_context.panel) != ESP_OK ||
        esp_lcd_panel_reset(s_context.panel) != ESP_OK || mybot_cores3_reset_display() < 0 ||
        esp_lcd_panel_init(s_context.panel) != ESP_OK ||
        esp_lcd_panel_invert_color(s_context.panel, true) != ESP_OK ||
        esp_lcd_panel_disp_on_off(s_context.panel, true) != ESP_OK ||
        mybot_cores3_set_display_backlight(100) < 0) {
        release_lcd();
        ESP_LOGE(TAG, "event=lcd action=initialize result=error");
        return -1;
    }

    s_context.initialized = true;
    s_context.users = 1;
    *out_ctx = &s_context;
    ESP_LOGI(TAG, "event=lcd action=initialize result=ok width=%d height=%d", MYBOT_DISPLAY_WIDTH,
             MYBOT_DISPLAY_HEIGHT);
    return 0;
}

static int lcd_render(void *opaque, const mybot_lcd_content_t *content) {
    lcd_context_t *ctx = opaque;
    if (ctx != &s_context || !ctx->initialized || !content) {
        return -1;
    }

    const char *label = NULL;
    uint16_t background = 0;
    switch (content->screen) {
    case MYBOT_LCD_SCREEN_STARTING:
        label = "STARTING";
        background = RGB565(22, 52, 74);
        break;
    case MYBOT_LCD_SCREEN_WIFI_PROVISIONING:
        label = "WIFI SETUP";
        background = RGB565(44, 61, 32);
        break;
    case MYBOT_LCD_SCREEN_WIFI_DISCONNECTED:
        label = "WIFI LOST";
        background = RGB565(86, 51, 18);
        break;
    case MYBOT_LCD_SCREEN_STARTING_SERVICES:
        label = "SERVICES";
        background = RGB565(36, 51, 74);
        break;
    case MYBOT_LCD_SCREEN_PAIRING:
        label = "PAIRING";
        background = RGB565(68, 49, 83);
        break;
    case MYBOT_LCD_SCREEN_PAIR_CODE:
        label = "PAIR CODE";
        background = RGB565(68, 49, 83);
        break;
    case MYBOT_LCD_SCREEN_READY:
        label = "READY";
        background = RGB565(23, 73, 46);
        break;
    case MYBOT_LCD_SCREEN_IN_CONVERSATION:
        label = "TALKING";
        background = RGB565(18, 66, 78);
        break;
    case MYBOT_LCD_SCREEN_FAILED:
        label = "FAILED";
        background = RGB565(104, 30, 30);
        break;
    case MYBOT_LCD_SCREEN_STOPPING:
        label = "STOPPING";
        background = RGB565(52, 52, 52);
        break;
    default:
        return -1;
    }

    const uint16_t foreground = RGB565(250, 250, 248);
    if (fill_screen(background) < 0 ||
        draw_text_centered(content->screen == MYBOT_LCD_SCREEN_PAIR_CODE ? 62 : 104, label, 3,
                           foreground, background) < 0) {
        return -1;
    }
    if (content->screen == MYBOT_LCD_SCREEN_PAIR_CODE) {
        const size_t code_length = strlen(content->pair_code);
        const int code_scale = code_length <= 11 ? 4 : 3;
        return draw_text_centered(128, content->pair_code, code_scale, foreground, background);
    }
    if (content->screen == MYBOT_LCD_SCREEN_IN_CONVERSATION &&
        (content->indicators & MYBOT_LCD_INDICATOR_VP_REGISTERED) != 0) {
        return draw_text_centered(145, "VP SAVED", 2, RGB565(114, 255, 156), background);
    }
    return 0;
}

static void lcd_destroy(void *opaque) {
    lcd_context_t *ctx = opaque;
    if (ctx != &s_context || !ctx->initialized || ctx->users == 0) {
        return;
    }
    ESP_LOGI(TAG, "event=lcd action=detach references=%u", ctx->users - 1);
    if (--ctx->users > 0) {
        return;
    }
    (void)mybot_cores3_set_display_backlight(0);
    (void)esp_lcd_panel_disp_on_off(ctx->panel, false);
    release_lcd();
    *ctx = (lcd_context_t){0};
}

static const mybot_lcd_ops_t s_ops = {
    .init = lcd_init,
    .render = lcd_render,
    .destroy = lcd_destroy,
};

const mybot_lcd_ops_t *mybot_cores3_lcd_ops(void) {
    return &s_ops;
}
