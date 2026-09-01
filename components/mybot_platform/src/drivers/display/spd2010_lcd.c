/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Project Contributors */
#include "board_config.h"

#include <mybot/platform/mybot_lcd.h>

#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_spd2010.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sensecap_hardware.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define TAG "sensecap_lcd"
#define LCD_TRANSFER_ROWS 16
#define LCD_TRANSFER_TIMEOUT_MS 1000

#define RGB565(red, green, blue)                                                                   \
    __builtin_bswap16((uint16_t)((((red) & 0xf8) << 8) | (((green) & 0xfc) << 3) | ((blue) >> 3)))

typedef struct {
    const char *text;
    int y;
    int scale;
    uint16_t color;
} text_layer_t;

typedef struct {
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
    SemaphoreHandle_t transfer_done;
    bool spi_ready;
    bool initialized;
    bool transfer_failed;
    bool color_in_flight;
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
    lcd_context_t *context = user_data;
    BaseType_t high_priority_woken = pdFALSE;
    xSemaphoreGiveFromISR(context->transfer_done, &high_priority_woken);
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

static int draw_text_into_strip(int strip_y, int rows, const text_layer_t *layer) {
    const size_t length = strlen(layer->text);
    const int text_width = (int)length * 6 * layer->scale;
    const int text_x = (MYBOT_DISPLAY_WIDTH - text_width) / 2;
    if (text_x < 0 || layer->scale <= 0) {
        return -1;
    }

    for (size_t character = 0; character < length; ++character) {
        const uint8_t *glyph = glyph_for(layer->text[character]);
        const int character_x = text_x + (int)character * 6 * layer->scale;
        for (int local_y = 0; local_y < rows; ++local_y) {
            const int text_y = strip_y + local_y - layer->y;
            if (text_y < 0 || text_y >= 7 * layer->scale) {
                continue;
            }
            const int glyph_y = text_y / layer->scale;
            for (int pixel_x = 0; pixel_x < 5 * layer->scale; ++pixel_x) {
                const int glyph_x = pixel_x / layer->scale;
                if ((glyph[glyph_x] & (1U << glyph_y)) != 0) {
                    s_draw_buffer[local_y * MYBOT_DISPLAY_WIDTH + character_x + pixel_x] =
                        layer->color;
                }
            }
        }
    }
    return 0;
}

static int submit_strip(int y, int rows) {
    if (s_context.transfer_failed) {
        return -1;
    }
    while (xSemaphoreTake(s_context.transfer_done, 0) == pdTRUE) {
    }
    s_context.color_in_flight = true;
    if (esp_lcd_panel_draw_bitmap(s_context.panel, 0, y, MYBOT_DISPLAY_WIDTH, y + rows,
                                  s_draw_buffer) != ESP_OK) {
        s_context.color_in_flight = false;
        return -1;
    }
    if (xSemaphoreTake(s_context.transfer_done, pdMS_TO_TICKS(LCD_TRANSFER_TIMEOUT_MS)) != pdTRUE) {
        s_context.transfer_failed = true;
        ESP_LOGE(TAG, "event=lcd action=transfer result=error reason=timeout");
        return -1;
    }
    s_context.color_in_flight = false;
    return 0;
}

static int render_layers(uint16_t background, const text_layer_t *layers, size_t layer_count) {
    for (int y = 0; y < MYBOT_DISPLAY_HEIGHT; y += LCD_TRANSFER_ROWS) {
        int rows = MYBOT_DISPLAY_HEIGHT - y;
        if (rows > LCD_TRANSFER_ROWS) {
            rows = LCD_TRANSFER_ROWS;
        }
        for (int pixel = 0; pixel < MYBOT_DISPLAY_WIDTH * rows; ++pixel) {
            s_draw_buffer[pixel] = background;
        }
        for (size_t layer = 0; layer < layer_count; ++layer) {
            if (draw_text_into_strip(y, rows, &layers[layer]) < 0) {
                return -1;
            }
        }
        if (submit_strip(y, rows) < 0) {
            return -1;
        }
    }
    return 0;
}

static bool transport_is_present(void) {
    return s_context.panel || s_context.io || s_context.spi_ready || s_context.transfer_done;
}

static bool color_transfer_is_complete(void) {
    if (!s_context.color_in_flight) {
        return true;
    }
    if (!s_context.transfer_done || xSemaphoreTake(s_context.transfer_done, 0) != pdTRUE) {
        return false;
    }
    s_context.color_in_flight = false;
    return true;
}

static int release_transport(bool power_off) {
    if (!color_transfer_is_complete()) {
        ESP_LOGW(TAG, "event=lcd action=destroy result=deferred reason=transfer_still_in_flight");
        return -1;
    }
    if (mybot_sensecap_set_display_backlight(0) < 0) {
        ESP_LOGW(TAG, "event=lcd action=backlight_off result=error");
    }
    if (s_context.panel) {
        if (!s_context.transfer_failed &&
            esp_lcd_panel_disp_on_off(s_context.panel, false) != ESP_OK) {
            ESP_LOGW(TAG, "event=lcd action=display_off result=error");
        }
        esp_err_t result = esp_lcd_panel_del(s_context.panel);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "event=lcd action=destroy component=panel result=error code=%s",
                     esp_err_to_name(result));
            return -1;
        }
        s_context.panel = NULL;
    }
    if (s_context.io) {
        esp_err_t result = esp_lcd_panel_io_del(s_context.io);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "event=lcd action=destroy component=panel_io result=error code=%s",
                     esp_err_to_name(result));
            return -1;
        }
        s_context.io = NULL;
    }
    if (s_context.spi_ready) {
        esp_err_t result = spi_bus_free(MYBOT_SENSECAP_LCD_SPI_HOST);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "event=lcd action=destroy component=spi_bus result=error code=%s",
                     esp_err_to_name(result));
            return -1;
        }
        s_context.spi_ready = false;
    }
    if (s_context.transfer_done) {
        vSemaphoreDelete(s_context.transfer_done);
        s_context.transfer_done = NULL;
    }
    if (power_off && mybot_sensecap_set_lcd_power(false) < 0) {
        ESP_LOGW(TAG, "event=lcd action=power_off result=error");
    }
    return 0;
}

static int initialize_transport(void) {
    if (transport_is_present() && release_transport(true) < 0) {
        return -1;
    }
    if (mybot_sensecap_set_lcd_power(true) < 0) {
        return -1;
    }
    s_context.transfer_done = xSemaphoreCreateBinary();
    if (!s_context.transfer_done) {
        (void)release_transport(true);
        return -1;
    }

    const spi_bus_config_t bus_config = {
        .sclk_io_num = MYBOT_DISPLAY_PCLK,
        .data0_io_num = MYBOT_DISPLAY_DATA0,
        .data1_io_num = MYBOT_DISPLAY_DATA1,
        .data2_io_num = MYBOT_DISPLAY_DATA2,
        .data3_io_num = MYBOT_DISPLAY_DATA3,
        .max_transfer_sz = sizeof(s_draw_buffer),
    };
    if (spi_bus_initialize(MYBOT_SENSECAP_LCD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO) != ESP_OK) {
        (void)release_transport(true);
        return -1;
    }
    s_context.spi_ready = true;

    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = MYBOT_DISPLAY_CS,
        .dc_gpio_num = GPIO_NUM_NC,
        .spi_mode = 3,
        .pclk_hz = MYBOT_SENSECAP_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 2,
        .on_color_trans_done = on_color_transfer_done,
        .user_ctx = &s_context,
        .lcd_cmd_bits = MYBOT_SENSECAP_LCD_COMMAND_BITS,
        .lcd_param_bits = MYBOT_SENSECAP_LCD_PARAMETER_BITS,
        .flags.quad_mode = true,
    };
    if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)MYBOT_SENSECAP_LCD_SPI_HOST, &io_config,
                                 &s_context.io) != ESP_OK) {
        (void)release_transport(true);
        return -1;
    }

    const spd2010_vendor_config_t vendor_config = {
        .flags.use_qspi_interface = true,
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = MYBOT_SENSECAP_LCD_RESET,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = (void *)&vendor_config,
    };
    if (esp_lcd_new_panel_spd2010(s_context.io, &panel_config, &s_context.panel) != ESP_OK ||
        esp_lcd_panel_reset(s_context.panel) != ESP_OK ||
        esp_lcd_panel_init(s_context.panel) != ESP_OK ||
        esp_lcd_panel_mirror(s_context.panel, MYBOT_DISPLAY_MIRROR_X, MYBOT_DISPLAY_MIRROR_Y) !=
            ESP_OK ||
        esp_lcd_panel_disp_on_off(s_context.panel, true) != ESP_OK ||
        mybot_sensecap_set_display_backlight(100) < 0) {
        s_context.transfer_failed = true;
        if (release_transport(true) < 0) {
            ESP_LOGE(TAG, "event=lcd action=initialize cleanup=error");
        }
        ESP_LOGE(TAG, "event=lcd action=initialize result=error");
        return -1;
    }

    s_context.transfer_failed = false;
    return 0;
}

static int recover_transport(void) {
    ESP_LOGW(TAG, "event=lcd action=recover phase=begin");
    s_context.transfer_failed = true;
    if (release_transport(true) < 0 || initialize_transport() < 0) {
        ESP_LOGE(TAG, "event=lcd action=recover result=error");
        return -1;
    }
    ESP_LOGI(TAG, "event=lcd action=recover result=ok");
    return 0;
}

static int lcd_init(void **out_context) {
    if (!out_context || !mybot_sensecap_i2c_bus_handle()) {
        return -1;
    }
    *out_context = NULL;
    if (s_context.initialized) {
        if ((s_context.transfer_failed || !s_context.panel || !s_context.io) &&
            recover_transport() < 0) {
            return -1;
        }
        ++s_context.users;
        *out_context = &s_context;
        ESP_LOGI(TAG, "event=lcd action=attach references=%u", s_context.users);
        return 0;
    }

    if (initialize_transport() < 0) {
        return -1;
    }

    s_context.initialized = true;
    s_context.users = 1;
    *out_context = &s_context;
    ESP_LOGI(TAG, "event=lcd action=initialize result=ok width=%d height=%d bus=qspi",
             MYBOT_DISPLAY_WIDTH, MYBOT_DISPLAY_HEIGHT);
    return 0;
}

static int lcd_render(void *opaque, const mybot_lcd_content_t *content) {
    lcd_context_t *context = opaque;
    if (context != &s_context || !context->initialized || !content) {
        return -1;
    }
    if ((context->transfer_failed || !context->panel || !context->io) && recover_transport() < 0) {
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
    text_layer_t layers[2] = {
        {
            .text = label,
            .y = content->screen == MYBOT_LCD_SCREEN_PAIR_CODE ? 120 : 170,
            .scale = 4,
            .color = foreground,
        },
    };
    size_t layer_count = 1;
    if (content->screen == MYBOT_LCD_SCREEN_PAIR_CODE) {
        const size_t code_length = strlen(content->pair_code);
        layers[1] = (text_layer_t){
            .text = content->pair_code,
            .y = 230,
            .scale = code_length <= 11 ? 5 : 4,
            .color = foreground,
        };
        layer_count = 2;
    } else if (content->screen == MYBOT_LCD_SCREEN_IN_CONVERSATION) {
        const bool vp_registered = (content->indicators & MYBOT_LCD_INDICATOR_VP_REGISTERED) != 0;
        layers[1] = (text_layer_t){
            .text = vp_registered ? "VP SAVED" : "VP REGISTERING",
            .y = 245,
            .scale = 3,
            .color = vp_registered ? RGB565(114, 255, 156) : RGB565(255, 211, 92),
        };
        layer_count = 2;
    }
    return render_layers(background, layers, layer_count);
}

static void lcd_destroy(void *opaque) {
    lcd_context_t *context = opaque;
    if (context != &s_context || !context->initialized || context->users == 0) {
        return;
    }
    if (context->users > 1) {
        --context->users;
        ESP_LOGI(TAG, "event=lcd action=detach references=%u", context->users);
        return;
    }

    ESP_LOGI(TAG, "event=lcd action=detach references=0");
    if (release_transport(true) < 0) {
        ESP_LOGE(TAG, "event=lcd action=detach result=error reason=transport_release");
        return;
    }
    *context = (lcd_context_t){0};
}

static const mybot_lcd_ops_t s_ops = {
    .init = lcd_init,
    .render = lcd_render,
    .destroy = lcd_destroy,
};

const mybot_lcd_ops_t *mybot_sensecap_lcd_ops(void) {
    return &s_ops;
}
