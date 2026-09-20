/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#include "board_config.h"

#include <mybot/platform/mybot_lcd.h>

#include "cores3_hardware.h"
#include "cores3_lcd_panel.h"
#if CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR
#include "mybot_debug_stats.h"
#endif
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define TAG "cores3_lcd"
#define LCD_TRANSFER_ROWS CORES3_LCD_TRANSFER_ROWS
#define LCD_TRANSFER_TIMEOUT_MS 1000

#define LCD_LOGICAL_WIDTH MYBOT_DISPLAY_WIDTH
#define LCD_LOGICAL_HEIGHT MYBOT_DISPLAY_HEIGHT
#define LCD_FRAME_BYTES ((size_t)LCD_LOGICAL_WIDTH * LCD_LOGICAL_HEIGHT * sizeof(uint16_t))
#define LCD_CONVERSATION_CACHE_COUNT 8

#define LCD_LABEL_BASELINE_Y 178
#define LCD_PAIR_LABEL_BASELINE_Y 58
#define LCD_PAIR_CODE_BASELINE_Y 142
#define LCD_BADGE_CENTER_Y 46
#define LCD_BADGE_LEFT_X 108
#define LCD_BADGE_RIGHT_X 212
#define LCD_STATE_CENTER_X 160
#define LCD_STATE_CENTER_Y 88
#define LCD_STATE_RING_RADIUS 54
#define LCD_STATE_RING_THICKNESS 4

#define COLOR_BLACK 0x0000
#define COLOR_NEAR_BLACK 0x0841
#define COLOR_WHITE 0xffff
#define COLOR_CYAN 0x3fff
#define COLOR_BLUE 0x4a7f
#define COLOR_GREEN 0x47e8
#define COLOR_YELLOW 0xffe0
#define COLOR_AMBER 0xfd20
#define COLOR_RED 0xf986
#define COLOR_GRAY 0x8410
#define COLOR_VP_SAVED 0x77f3

#define RGB565(red, green, blue)                                                                   \
    (uint16_t)((((red) & 0xf8) << 8) | (((green) & 0xfc) << 3) | ((blue) >> 3))

typedef struct {
    cores3_lcd_panel_t lcd;
    SemaphoreHandle_t transfer_done;
    uint16_t *frame;
    uint16_t *screen_cache[MYBOT_LCD_SCREEN_COUNT];
    uint16_t *conversation_cache[LCD_CONVERSATION_CACHE_COUNT];
    bool initialized;
    bool transfer_failed;
    unsigned int users;
} lcd_context_t;

static lcd_context_t s_context;
static DMA_ATTR uint16_t s_draw_buffer[MYBOT_DISPLAY_WIDTH * LCD_TRANSFER_ROWS];

typedef struct {
    uint16_t data_offset;
    uint8_t character;
    uint8_t width;
    uint8_t height;
    uint8_t advance;
    int8_t x_offset;
    int8_t y_offset;
} lcd_font_glyph_t;

typedef struct {
    const uint8_t *bitmap;
    const lcd_font_glyph_t *glyphs;
    size_t glyph_count;
    uint8_t tracking;
} lcd_font_t;

#include "ili9342_lcd_font.inc"

static bool on_color_transfer_done(esp_lcd_panel_io_handle_t panel_io,
                                   esp_lcd_panel_io_event_data_t *event_data, void *user_data) {
    (void)panel_io;
    (void)event_data;
    lcd_context_t *ctx = user_data;
    BaseType_t high_priority_woken = pdFALSE;
    xSemaphoreGiveFromISR(ctx->transfer_done, &high_priority_woken);
    return high_priority_woken == pdTRUE;
}

static int submit_bitmap(int x, int y, int width, int height) {
    if (s_context.transfer_failed) {
        return -1;
    }
    while (xSemaphoreTake(s_context.transfer_done, 0) == pdTRUE) {
    }
    if (esp_lcd_panel_draw_bitmap(s_context.lcd.panel, x, y, x + width, y + height,
                                  s_draw_buffer) != ESP_OK) {
        return -1;
    }
    if (xSemaphoreTake(s_context.transfer_done, pdMS_TO_TICKS(LCD_TRANSFER_TIMEOUT_MS)) != pdTRUE) {
        s_context.transfer_failed = true;
        ESP_LOGE(TAG, "event=lcd action=transfer result=error reason=timeout");
        return -1;
    }
    return 0;
}

static int flush_frame(const uint16_t *frame) {
    if (!frame) {
        return -1;
    }
    for (int y = 0; y < LCD_LOGICAL_HEIGHT; y += LCD_TRANSFER_ROWS) {
        int rows = LCD_LOGICAL_HEIGHT - y;
        if (rows > LCD_TRANSFER_ROWS) {
            rows = LCD_TRANSFER_ROWS;
        }
        const uint16_t *source = frame + (size_t)y * LCD_LOGICAL_WIDTH;
        const size_t pixels = (size_t)rows * LCD_LOGICAL_WIDTH;
        /* Keep native RGB565 for blending; SPI sends each pixel's high byte first. */
        for (size_t pixel = 0; pixel < pixels; ++pixel) {
            s_draw_buffer[pixel] = __builtin_bswap16(source[pixel]);
        }
        if (submit_bitmap(0, y, LCD_LOGICAL_WIDTH, rows) < 0) {
            return -1;
        }
    }
    return 0;
}

static void fill_screen(uint16_t *frame, uint16_t color) {
    for (size_t i = 0; i < (size_t)LCD_LOGICAL_WIDTH * LCD_LOGICAL_HEIGHT; ++i) {
        frame[i] = color;
    }
}

static bool point_in_capsule(int point_x8, int point_y8, int start_x8, int start_y8, int end_x8,
                             int end_y8, int radius_squared) {
    int vector_x = end_x8 - start_x8;
    int vector_y = end_y8 - start_y8;
    int point_vector_x = point_x8 - start_x8;
    int point_vector_y = point_y8 - start_y8;
    int length_squared = vector_x * vector_x + vector_y * vector_y;
    int projection = point_vector_x * vector_x + point_vector_y * vector_y;

    if (projection <= 0 || length_squared == 0) {
        return point_vector_x * point_vector_x + point_vector_y * point_vector_y <= radius_squared;
    }
    if (projection >= length_squared) {
        int end_dx = point_x8 - end_x8;
        int end_dy = point_y8 - end_y8;
        return end_dx * end_dx + end_dy * end_dy <= radius_squared;
    }

    int64_t cross = (int64_t)point_vector_x * vector_y - (int64_t)point_vector_y * vector_x;
    return cross * cross <= (int64_t)radius_squared * length_squared;
}

static void blend_pixel(uint16_t *frame, int x, int y, uint16_t color, uint8_t alpha) {
    if (alpha == 0 || x < 0 || x >= LCD_LOGICAL_WIDTH || y < 0 || y >= LCD_LOGICAL_HEIGHT) {
        return;
    }

    uint16_t *pixel = &frame[(size_t)y * LCD_LOGICAL_WIDTH + x];
    if (alpha == 255) {
        *pixel = color;
        return;
    }

    uint32_t inverse = 255U - alpha;
    uint32_t red =
        ((((color >> 11) & 0x1fU) * alpha + ((*pixel >> 11) & 0x1fU) * inverse + 127U) / 255U)
        << 11;
    uint32_t green =
        ((((color >> 5) & 0x3fU) * alpha + ((*pixel >> 5) & 0x3fU) * inverse + 127U) / 255U) << 5;
    uint32_t blue = ((color & 0x1fU) * alpha + (*pixel & 0x1fU) * inverse + 127U) / 255U;
    *pixel = (uint16_t)(red | green | blue);
}

static void draw_line(uint16_t *frame, int x0, int y0, int x1, int y1, int thickness,
                      uint16_t color) {
    static const int sample_offsets[4] = {-3, -1, 1, 3};
    if (!frame || thickness <= 0) {
        return;
    }

    int padding = (thickness + 1) / 2 + 1;
    int min_x = (x0 < x1 ? x0 : x1) - padding;
    int max_x = (x0 > x1 ? x0 : x1) + padding;
    int min_y = (y0 < y1 ? y0 : y1) - padding;
    int max_y = (y0 > y1 ? y0 : y1) + padding;
    if (min_x < 0) {
        min_x = 0;
    }
    if (max_x >= LCD_LOGICAL_WIDTH) {
        max_x = LCD_LOGICAL_WIDTH - 1;
    }
    if (min_y < 0) {
        min_y = 0;
    }
    if (max_y >= LCD_LOGICAL_HEIGHT) {
        max_y = LCD_LOGICAL_HEIGHT - 1;
    }

    int start_x8 = x0 * 8;
    int start_y8 = y0 * 8;
    int end_x8 = x1 * 8;
    int end_y8 = y1 * 8;
    int radius8 = thickness * 4;
    int radius_squared = radius8 * radius8;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            unsigned int covered = 0;
            for (size_t sample_y = 0; sample_y < 4; ++sample_y) {
                int point_y8 = y * 8 + sample_offsets[sample_y];
                for (size_t sample_x = 0; sample_x < 4; ++sample_x) {
                    int point_x8 = x * 8 + sample_offsets[sample_x];
                    if (point_in_capsule(point_x8, point_y8, start_x8, start_y8, end_x8, end_y8,
                                         radius_squared)) {
                        ++covered;
                    }
                }
            }
            blend_pixel(frame, x, y, color, (uint8_t)((covered * 255U + 8U) >> 4));
        }
    }
}

static void draw_radial(uint16_t *frame, int center_x, int center_y, int inner_radius,
                        int outer_radius, uint16_t color) {
    static const int sample_offsets[4] = {-3, -1, 1, 3};
    if (!frame || outer_radius <= 0) {
        return;
    }

    int min_x = center_x - outer_radius;
    int max_x = center_x + outer_radius;
    int min_y = center_y - outer_radius;
    int max_y = center_y + outer_radius;
    if (min_x < 0) {
        min_x = 0;
    }
    if (max_x >= LCD_LOGICAL_WIDTH) {
        max_x = LCD_LOGICAL_WIDTH - 1;
    }
    if (min_y < 0) {
        min_y = 0;
    }
    if (max_y >= LCD_LOGICAL_HEIGHT) {
        max_y = LCD_LOGICAL_HEIGHT - 1;
    }

    int outer8 = outer_radius * 8;
    int outer_squared = outer8 * outer8;
    int inner8 = inner_radius > 0 ? inner_radius * 8 : 0;
    int inner_squared = inner8 * inner8;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            unsigned int covered = 0;
            for (size_t sample_y = 0; sample_y < 4; ++sample_y) {
                int dy8 = (y - center_y) * 8 + sample_offsets[sample_y];
                for (size_t sample_x = 0; sample_x < 4; ++sample_x) {
                    int dx8 = (x - center_x) * 8 + sample_offsets[sample_x];
                    int distance_squared = dx8 * dx8 + dy8 * dy8;
                    if (distance_squared <= outer_squared &&
                        (inner_radius <= 0 || distance_squared >= inner_squared)) {
                        ++covered;
                    }
                }
            }
            blend_pixel(frame, x, y, color, (uint8_t)((covered * 255U + 8U) >> 4));
        }
    }
}

static void draw_ring(uint16_t *frame, int center_x, int center_y, int radius, int thickness,
                      uint16_t color) {
    int inner_radius = radius - thickness;
    draw_radial(frame, center_x, center_y, inner_radius > 0 ? inner_radius : 0, radius, color);
}

static void draw_disc(uint16_t *frame, int center_x, int center_y, int radius, uint16_t color) {
    draw_radial(frame, center_x, center_y, 0, radius, color);
}

static const lcd_font_glyph_t *font_glyph(const lcd_font_t *font, char character) {
    if (!font) {
        return NULL;
    }

    const lcd_font_glyph_t *fallback = NULL;
    for (size_t index = 0; index < font->glyph_count; ++index) {
        if (font->glyphs[index].character == (uint8_t)character) {
            return &font->glyphs[index];
        }
        if (font->glyphs[index].character == '?') {
            fallback = &font->glyphs[index];
        }
    }
    return fallback;
}

static int text_width(const lcd_font_t *font, const char *text, size_t length) {
    int width = 0;
    size_t glyphs = 0;

    for (size_t index = 0; index < length; ++index) {
        const lcd_font_glyph_t *glyph = font_glyph(font, text[index]);
        if (!glyph) {
            continue;
        }
        if (glyphs != 0) {
            width += font->tracking;
        }
        width += glyph->advance;
        ++glyphs;
    }
    return width;
}

static void draw_character(uint16_t *frame, int x, int baseline_y, const lcd_font_t *font,
                           const lcd_font_glyph_t *glyph, uint16_t color) {
    if (!frame || !font || !glyph) {
        return;
    }

    size_t stride = ((size_t)glyph->width + 1U) / 2U;
    const uint8_t *bitmap = font->bitmap + glyph->data_offset;
    for (uint8_t row = 0; row < glyph->height; ++row) {
        const uint8_t *bitmap_row = bitmap + (size_t)row * stride;
        for (uint8_t column = 0; column < glyph->width; ++column) {
            uint8_t packed = bitmap_row[column / 2U];
            uint8_t coverage = (column & 1U) == 0 ? packed >> 4 : packed & 0x0fU;
            blend_pixel(frame, x + glyph->x_offset + column, baseline_y + glyph->y_offset + row,
                        color, (uint8_t)(coverage * 17U));
        }
    }
}

static void draw_text_centered(uint16_t *frame, int baseline_y, const char *text, size_t length,
                               const lcd_font_t *font, uint16_t color) {
    int pen_x = (LCD_LOGICAL_WIDTH - text_width(font, text, length)) / 2;
    size_t glyphs = 0;

    for (size_t index = 0; index < length; ++index) {
        const lcd_font_glyph_t *glyph = font_glyph(font, text[index]);
        if (!glyph) {
            continue;
        }
        if (glyphs != 0) {
            pen_x += font->tracking;
        }
        draw_character(frame, pen_x, baseline_y, font, glyph, color);
        pen_x += glyph->advance;
        ++glyphs;
    }
}

static uint16_t screen_color(mybot_lcd_screen_t screen) {
    switch (screen) {
    case MYBOT_LCD_SCREEN_STARTING:
        return COLOR_CYAN;
    case MYBOT_LCD_SCREEN_WIFI_PROVISIONING:
        return COLOR_AMBER;
    case MYBOT_LCD_SCREEN_WIFI_DISCONNECTED:
    case MYBOT_LCD_SCREEN_FAILED:
        return COLOR_RED;
    case MYBOT_LCD_SCREEN_STARTING_SERVICES:
        return COLOR_BLUE;
    case MYBOT_LCD_SCREEN_PAIRING:
        return COLOR_YELLOW;
    case MYBOT_LCD_SCREEN_READY:
        return COLOR_GREEN;
    case MYBOT_LCD_SCREEN_IN_CONVERSATION:
        return COLOR_CYAN;
    case MYBOT_LCD_SCREEN_STOPPING:
        return COLOR_GRAY;
    case MYBOT_LCD_SCREEN_PAIR_CODE:
    case MYBOT_LCD_SCREEN_COUNT:
        return COLOR_CYAN;
    }
    return COLOR_GRAY;
}

static const char *screen_label(mybot_lcd_screen_t screen) {
    switch (screen) {
    case MYBOT_LCD_SCREEN_STARTING:
        return "STARTING";
    case MYBOT_LCD_SCREEN_WIFI_PROVISIONING:
        return "WIFI SETUP";
    case MYBOT_LCD_SCREEN_WIFI_DISCONNECTED:
        return "WIFI LOST";
    case MYBOT_LCD_SCREEN_STARTING_SERVICES:
        return "SERVICES";
    case MYBOT_LCD_SCREEN_PAIRING:
        return "PAIRING";
    case MYBOT_LCD_SCREEN_PAIR_CODE:
        return "PAIR CODE";
    case MYBOT_LCD_SCREEN_READY:
        return "READY";
    case MYBOT_LCD_SCREEN_IN_CONVERSATION:
        return "CONVERSATION";
    case MYBOT_LCD_SCREEN_FAILED:
        return "FAILED";
    case MYBOT_LCD_SCREEN_STOPPING:
        return "STOPPING";
    case MYBOT_LCD_SCREEN_COUNT:
        return NULL;
    }
    return NULL;
}

static mybot_lcd_indicator_t server_indicator(uint32_t indicators) {
    if (indicators & MYBOT_LCD_INDICATOR_LISTENING) {
        return MYBOT_LCD_INDICATOR_LISTENING;
    }
    if (indicators & MYBOT_LCD_INDICATOR_THINKING) {
        return MYBOT_LCD_INDICATOR_THINKING;
    }
    if (indicators & MYBOT_LCD_INDICATOR_SPEAKING) {
        return MYBOT_LCD_INDICATOR_SPEAKING;
    }
    return MYBOT_LCD_INDICATOR_NONE;
}

static uint16_t server_indicator_color(mybot_lcd_indicator_t indicator) {
    switch (indicator) {
    case MYBOT_LCD_INDICATOR_LISTENING:
        return COLOR_CYAN;
    case MYBOT_LCD_INDICATOR_THINKING:
        return COLOR_AMBER;
    case MYBOT_LCD_INDICATOR_SPEAKING:
        return COLOR_GREEN;
    case MYBOT_LCD_INDICATOR_NONE:
    case MYBOT_LCD_INDICATOR_VP_REGISTERED:
        return COLOR_CYAN;
    }
    return COLOR_CYAN;
}

static void draw_server_state_overlay(uint16_t *frame, mybot_lcd_indicator_t indicator) {
    const int center_x = LCD_BADGE_LEFT_X;
    const int center_y = LCD_BADGE_CENTER_Y;

    if (indicator == MYBOT_LCD_INDICATOR_NONE || indicator == MYBOT_LCD_INDICATOR_VP_REGISTERED) {
        return;
    }

    draw_disc(frame, center_x, center_y, 15, server_indicator_color(indicator));
    switch (indicator) {
    case MYBOT_LCD_INDICATOR_LISTENING:
        draw_line(frame, center_x, center_y - 5, center_x, center_y + 1, 5, COLOR_BLACK);
        draw_line(frame, center_x - 6, center_y - 1, center_x - 6, center_y + 3, 2, COLOR_BLACK);
        draw_line(frame, center_x - 6, center_y + 3, center_x, center_y + 6, 2, COLOR_BLACK);
        draw_line(frame, center_x, center_y + 6, center_x + 6, center_y + 3, 2, COLOR_BLACK);
        draw_line(frame, center_x + 6, center_y + 3, center_x + 6, center_y - 1, 2, COLOR_BLACK);
        draw_line(frame, center_x, center_y + 6, center_x, center_y + 9, 2, COLOR_BLACK);
        break;
    case MYBOT_LCD_INDICATOR_THINKING:
        draw_disc(frame, center_x - 7, center_y, 3, COLOR_BLACK);
        draw_disc(frame, center_x, center_y, 3, COLOR_BLACK);
        draw_disc(frame, center_x + 7, center_y, 3, COLOR_BLACK);
        break;
    case MYBOT_LCD_INDICATOR_SPEAKING:
        draw_line(frame, center_x - 7, center_y - 3, center_x - 7, center_y + 3, 5, COLOR_BLACK);
        draw_line(frame, center_x - 4, center_y - 4, center_x, center_y - 7, 3, COLOR_BLACK);
        draw_line(frame, center_x - 4, center_y + 4, center_x, center_y + 7, 3, COLOR_BLACK);
        draw_line(frame, center_x, center_y - 7, center_x, center_y + 7, 3, COLOR_BLACK);
        draw_line(frame, center_x + 4, center_y - 5, center_x + 8, center_y - 8, 2, COLOR_BLACK);
        draw_line(frame, center_x + 4, center_y + 5, center_x + 8, center_y + 8, 2, COLOR_BLACK);
        break;
    case MYBOT_LCD_INDICATOR_NONE:
    case MYBOT_LCD_INDICATOR_VP_REGISTERED:
        break;
    }
}

static void draw_voiceprint_glyph(uint16_t *frame, int center_x, int center_y, uint16_t color) {
    draw_line(frame, center_x - 9, center_y, center_x - 6, center_y, 3, color);
    draw_line(frame, center_x - 6, center_y, center_x - 3, center_y - 4, 3, color);
    draw_line(frame, center_x - 3, center_y - 4, center_x, center_y + 5, 3, color);
    draw_line(frame, center_x, center_y + 5, center_x + 3, center_y - 7, 3, color);
    draw_line(frame, center_x + 3, center_y - 7, center_x + 6, center_y + 3, 3, color);
    draw_line(frame, center_x + 6, center_y + 3, center_x + 9, center_y, 3, color);
}

static void draw_voiceprint_overlay(uint16_t *frame, bool registered) {
    const int center_x = LCD_BADGE_RIGHT_X;
    const int center_y = LCD_BADGE_CENTER_Y;

    draw_disc(frame, center_x, center_y, 15, registered ? COLOR_VP_SAVED : COLOR_RED);
    draw_voiceprint_glyph(frame, center_x, center_y, COLOR_BLACK);
}

static int pair_code_length(const char *code, size_t *out_length) {
    if (!code || !out_length) {
        return -1;
    }

    size_t length = 0;
    while (length < MYBOT_LCD_PAIR_CODE_CAPACITY && code[length] != '\0') {
        ++length;
    }
    if (length != 6) {
        return -1;
    }
    for (size_t index = 0; index < length; ++index) {
        if (code[index] < '0' || code[index] > '9') {
            return -1;
        }
    }
    *out_length = length;
    return 0;
}

static void draw_state_icon(uint16_t *frame, mybot_lcd_screen_t screen, uint16_t color) {
    const int center_x = LCD_STATE_CENTER_X;
    const int center_y = LCD_STATE_CENTER_Y;

    draw_ring(frame, center_x, center_y, LCD_STATE_RING_RADIUS, LCD_STATE_RING_THICKNESS, color);
    switch (screen) {
    case MYBOT_LCD_SCREEN_STARTING:
    case MYBOT_LCD_SCREEN_STARTING_SERVICES:
        draw_line(frame, center_x, center_y - 28, center_x, center_y, 7, color);
        draw_line(frame, center_x, center_y, center_x + 21, center_y + 15, 7, color);
        break;
    case MYBOT_LCD_SCREEN_WIFI_PROVISIONING:
    case MYBOT_LCD_SCREEN_PAIRING:
        draw_disc(frame, center_x - 24, center_y, 7, color);
        draw_disc(frame, center_x, center_y, 7, color);
        draw_disc(frame, center_x + 24, center_y, 7, color);
        break;
    case MYBOT_LCD_SCREEN_WIFI_DISCONNECTED:
    case MYBOT_LCD_SCREEN_FAILED:
        draw_line(frame, center_x - 21, center_y - 21, center_x + 21, center_y + 21, 7, color);
        draw_line(frame, center_x + 21, center_y - 21, center_x - 21, center_y + 21, 7, color);
        break;
    case MYBOT_LCD_SCREEN_READY:
        draw_line(frame, center_x - 27, center_y, center_x - 8, center_y + 20, 7, color);
        draw_line(frame, center_x - 8, center_y + 20, center_x + 32, center_y - 23, 7, color);
        break;
    case MYBOT_LCD_SCREEN_IN_CONVERSATION:
        draw_line(frame, center_x - 25, center_y - 13, center_x - 25, center_y + 13, 9, color);
        draw_line(frame, center_x, center_y - 26, center_x, center_y + 26, 9, color);
        draw_line(frame, center_x + 25, center_y - 13, center_x + 25, center_y + 13, 9, color);
        break;
    case MYBOT_LCD_SCREEN_STOPPING:
        draw_line(frame, center_x - 23, center_y, center_x + 23, center_y, 8, color);
        break;
    case MYBOT_LCD_SCREEN_PAIR_CODE:
    case MYBOT_LCD_SCREEN_COUNT:
        break;
    }
}

static int render_content(uint16_t *frame, const mybot_lcd_content_t *content) {
    if (!frame || !content || content->screen < MYBOT_LCD_SCREEN_STARTING ||
        content->screen >= MYBOT_LCD_SCREEN_COUNT) {
        return -1;
    }

    fill_screen(frame,
                content->screen == MYBOT_LCD_SCREEN_PAIR_CODE ? COLOR_BLACK : COLOR_NEAR_BLACK);
    const char *label = screen_label(content->screen);
    if (!label) {
        return -1;
    }

    if (content->screen == MYBOT_LCD_SCREEN_PAIR_CODE) {
        size_t code_length;
        if (pair_code_length(content->pair_code, &code_length) < 0) {
            return -1;
        }
        draw_text_centered(frame, LCD_PAIR_LABEL_BASELINE_Y, label, strlen(label), &s_ui_label_font,
                           COLOR_CYAN);
        draw_text_centered(frame, LCD_PAIR_CODE_BASELINE_Y, content->pair_code, code_length,
                           &s_ui_digit_font, COLOR_WHITE);
        return 0;
    }

    uint16_t color = screen_color(content->screen);
    draw_state_icon(frame, content->screen, color);
    draw_text_centered(frame, LCD_LABEL_BASELINE_Y, label, strlen(label), &s_ui_label_font, color);
    if (content->screen == MYBOT_LCD_SCREEN_IN_CONVERSATION) {
        draw_server_state_overlay(frame, server_indicator(content->indicators));
        draw_voiceprint_overlay(frame,
                                (content->indicators & MYBOT_LCD_INDICATOR_VP_REGISTERED) != 0);
    }
    return 0;
}

static void release_cache(void) {
    for (int screen = 0; screen < MYBOT_LCD_SCREEN_COUNT; ++screen) {
        heap_caps_free(s_context.screen_cache[screen]);
        s_context.screen_cache[screen] = NULL;
    }
    for (int index = 0; index < LCD_CONVERSATION_CACHE_COUNT; ++index) {
        heap_caps_free(s_context.conversation_cache[index]);
        s_context.conversation_cache[index] = NULL;
    }
}

static int cache_content(uint16_t **destination, const mybot_lcd_content_t *content) {
    *destination = heap_caps_malloc(LCD_FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!*destination) {
        return -1;
    }
    const int result = render_content(*destination, content);
    /* Pre-render before networking, yielding between pages for the idle tasks. */
    vTaskDelay(pdMS_TO_TICKS(1));
    return result;
}

static void initialize_cache(void) {
    static const uint32_t states[] = {
        MYBOT_LCD_INDICATOR_NONE,
        MYBOT_LCD_INDICATOR_LISTENING,
        MYBOT_LCD_INDICATOR_THINKING,
        MYBOT_LCD_INDICATOR_SPEAKING,
    };
    const int64_t begin = esp_timer_get_time();
    unsigned int pages = 0;
    for (int screen = 0; screen < MYBOT_LCD_SCREEN_COUNT; ++screen) {
        if (screen == MYBOT_LCD_SCREEN_PAIR_CODE || screen == MYBOT_LCD_SCREEN_IN_CONVERSATION) {
            continue;
        }
        const mybot_lcd_content_t content = {.screen = (mybot_lcd_screen_t)screen};
        if (cache_content(&s_context.screen_cache[screen], &content) < 0) {
            goto fallback;
        }
        ++pages;
    }
    for (int index = 0; index < LCD_CONVERSATION_CACHE_COUNT; ++index) {
        const mybot_lcd_content_t content = {
            .screen = MYBOT_LCD_SCREEN_IN_CONVERSATION,
            .indicators = states[index / 2] | ((index & 1) ? MYBOT_LCD_INDICATOR_VP_REGISTERED : 0),
        };
        if (cache_content(&s_context.conversation_cache[index], &content) < 0) {
            goto fallback;
        }
        ++pages;
    }
    ESP_LOGI(TAG, "event=lcd_cache action=initialize pages=%u bytes=%zu elapsed_ms=%lld result=ok",
             pages, pages * LCD_FRAME_BYTES, (long long)((esp_timer_get_time() - begin) / 1000));
    return;

fallback:
    release_cache();
    ESP_LOGW(TAG, "event=lcd_cache action=initialize result=fallback mode=dynamic_render");
}

static const uint16_t *cached_frame(const mybot_lcd_content_t *content) {
    if (content->screen == MYBOT_LCD_SCREEN_IN_CONVERSATION) {
        unsigned int state = 0;
        switch (server_indicator(content->indicators)) {
        case MYBOT_LCD_INDICATOR_LISTENING:
            state = 1;
            break;
        case MYBOT_LCD_INDICATOR_THINKING:
            state = 2;
            break;
        case MYBOT_LCD_INDICATOR_SPEAKING:
            state = 3;
            break;
        default:
            break;
        }
        const unsigned int vp = !!(content->indicators & MYBOT_LCD_INDICATOR_VP_REGISTERED);
        return s_context.conversation_cache[state * 2 + vp];
    }
    return s_context.screen_cache[content->screen];
}

static int release_lcd(void) {
    if (mybot_cores3_lcd_panel_close(&s_context.lcd) < 0) {
        ESP_LOGE(TAG, "event=lcd action=cleanup result=error resources=retained");
        return -1;
    }
    if (s_context.transfer_done) {
        vSemaphoreDelete(s_context.transfer_done);
        s_context.transfer_done = NULL;
    }
    if (s_context.frame) {
        heap_caps_free(s_context.frame);
        s_context.frame = NULL;
    }
    release_cache();
    return 0;
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

    if (release_lcd() < 0) {
        return -1;
    }
    s_context = (lcd_context_t){0};

    s_context.transfer_done = xSemaphoreCreateBinary();
    if (!s_context.transfer_done) {
        return -1;
    }
    if (mybot_cores3_lcd_panel_open(&s_context.lcd, on_color_transfer_done, &s_context) < 0) {
        release_lcd();
        ESP_LOGE(TAG, "event=lcd action=initialize result=error");
        return -1;
    }

    s_context.frame = heap_caps_malloc(LCD_FRAME_BYTES, MALLOC_CAP_SPIRAM);
    if (!s_context.frame) {
        release_lcd();
        ESP_LOGE(TAG, "event=lcd action=initialize result=error reason=framebuffer");
        return -1;
    }

    initialize_cache();
    s_context.initialized = true;
    s_context.users = 1;
    *out_ctx = &s_context;
    ESP_LOGI(TAG, "event=lcd action=initialize result=ok width=%d height=%d", MYBOT_DISPLAY_WIDTH,
             MYBOT_DISPLAY_HEIGHT);
    return 0;
}

static int lcd_render(void *opaque, const mybot_lcd_content_t *content) {
    lcd_context_t *ctx = opaque;
    if (ctx != &s_context || !ctx->initialized || !content || !ctx->frame ||
        content->screen < MYBOT_LCD_SCREEN_STARTING || content->screen >= MYBOT_LCD_SCREEN_COUNT) {
        return -1;
    }
#if CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR
    const int64_t begin = esp_timer_get_time();
#endif
    const uint16_t *frame = cached_frame(content);
    int result = 0;
    if (!frame) {
        result = render_content(ctx->frame, content);
        frame = ctx->frame;
    }
#if CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR
    const int64_t rendered = esp_timer_get_time();
#endif
    if (result == 0) {
        result = flush_frame(frame);
    }
#if CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR
    mybot_debug_record_ui(begin, rendered, esp_timer_get_time(), result < 0);
#endif
    return result;
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
    (void)esp_lcd_panel_disp_on_off(ctx->lcd.panel, false);
    ctx->initialized = false;
    if (release_lcd() < 0) {
        return;
    }
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
