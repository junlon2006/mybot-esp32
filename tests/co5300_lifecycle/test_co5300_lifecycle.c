/* SPDX-License-Identifier: MIT */
#include "host_platform.h"

#include "board_config.h"
#include "display/display_panel.h"
#include "esp_lcd_co5300.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

struct host_panel_io {
    bool open;
};
struct host_panel {
    bool open;
    bool initialized;
    bool on;
    unsigned int brightness;
};

typedef enum {
    EVENT_SPI_NEW,
    EVENT_IO_NEW,
    EVENT_PANEL_NEW,
    EVENT_GAP,
    EVENT_RESET,
    EVENT_INIT,
    EVENT_INVERT,
    EVENT_SWAP,
    EVENT_MIRROR,
    EVENT_ON,
    EVENT_BRIGHTNESS_ON,
    EVENT_BRIGHTNESS_OFF,
    EVENT_OFF,
    EVENT_PANEL_DELETE,
    EVENT_IO_DELETE,
    EVENT_SPI_FREE,
} event_t;

static struct host_panel_io s_io;
static struct host_panel s_panel;
static bool s_spi_open;
static bool s_partial_constructor;
static const char *s_failure;
static const char *s_cleanup_failure;
static event_t s_events[64];
static size_t s_event_count;
static unsigned int s_spi_initializations;
static unsigned int s_vendor_checks;

static esp_err_t step(const char *name, event_t event) {
    CHECK(s_event_count < sizeof(s_events) / sizeof(s_events[0]));
    s_events[s_event_count++] = event;
    if (s_failure && !strcmp(s_failure, name)) {
        s_failure = NULL;
        return ESP_ERR_TIMEOUT;
    }
    if (s_cleanup_failure && !strcmp(s_cleanup_failure, name)) {
        s_cleanup_failure = NULL;
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static size_t event_position(event_t event) {
    for (size_t i = 0; i < s_event_count; ++i) {
        if (s_events[i] == event) {
            return i;
        }
    }
    CHECK(false);
    return 0;
}

static const co5300_lcd_init_cmd_t s_original_commands[] = {
    {0xfe, (uint8_t[]){0x20}, 1, 0},
    {0x19, (uint8_t[]){0x10}, 1, 0},
    {0x1c, (uint8_t[]){0xa0}, 1, 0},
    {0xfe, (uint8_t[]){0x00}, 1, 0},
    {0xc4, (uint8_t[]){0x80}, 1, 0},
    {0x3a, (uint8_t[]){0x55}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 0},
    {0x51, (uint8_t[]){0x00}, 1, 0},
    {0x63, (uint8_t[]){0xff}, 1, 0},
    {0x2a, (uint8_t[]){0x00, 0x06, 0x01, 0xd7}, 4, 0},
    {0x2b, (uint8_t[]){0x00, 0x00, 0x01, 0xd1}, 4, 600},
    {0x11, NULL, 0, 600},
    {0x29, NULL, 0, 0},
};

static const co5300_lcd_init_cmd_t s_216_commands[] = {
    {0x11, NULL, 0, 600},
    {0xfe, (uint8_t[]){0x20}, 1, 0},
    {0x19, (uint8_t[]){0x10}, 1, 0},
    {0x1c, (uint8_t[]){0xa0}, 1, 0},
    {0xfe, (uint8_t[]){0x00}, 1, 0},
    {0xc4, (uint8_t[]){0x80}, 1, 0},
    {0x3a, (uint8_t[]){0x55}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 0},
    {0x51, (uint8_t[]){0x00}, 1, 0},
    {0x63, (uint8_t[]){0xff}, 1, 0},
    {0x2a, (uint8_t[]){0x00, 0x00, 0x01, 0xdf}, 4, 0},
    {0x2b, (uint8_t[]){0x00, 0x00, 0x01, 0xdf}, 4, 0},
    {0x36, (uint8_t[]){0xa0}, 1, 0},
    {0x29, NULL, 0, 600},
};

static void check_command(const co5300_lcd_init_cmd_t *actual,
                          const co5300_lcd_init_cmd_t *expected) {
    CHECK(actual->cmd == expected->cmd && actual->data_bytes == expected->data_bytes);
    CHECK(actual->delay_ms == expected->delay_ms);
    if (actual->data_bytes) {
        CHECK(actual->data && !memcmp(actual->data, expected->data, actual->data_bytes));
    } else {
        CHECK(actual->data == NULL);
    }
}

static void check_vendor_commands(const co5300_vendor_config_t *vendor) {
    CHECK(vendor && vendor->flags.use_qspi_interface && vendor->init_cmds);
    if (!strcmp(MYBOT_BOARD_NAME, "esp32-s3-touch-amoled-2.16")) {
        CHECK(MYBOT_DISPLAY_WIDTH == 480 && MYBOT_DISPLAY_HEIGHT == 480);
        CHECK(MYBOT_DISPLAY_OFFSET_X == 0 && MYBOT_DISPLAY_OFFSET_Y == 0);
        CHECK(vendor->init_cmds_size == sizeof(s_216_commands) / sizeof(s_216_commands[0]));
        for (size_t i = 0; i < vendor->init_cmds_size; ++i) {
            check_command(&vendor->init_cmds[i], &s_216_commands[i]);
        }
    } else {
        CHECK(!strcmp(MYBOT_BOARD_NAME, "esp32-s3-touch-amoled-1.75") ||
              !strcmp(MYBOT_BOARD_NAME, "esp32-s3-touch-amoled-1.75c"));
        CHECK(MYBOT_DISPLAY_WIDTH == 466 && MYBOT_DISPLAY_HEIGHT == 466);
        CHECK(vendor->init_cmds_size ==
              sizeof(s_original_commands) / sizeof(s_original_commands[0]));
        for (size_t i = 0; i < vendor->init_cmds_size; ++i) {
            check_command(&vendor->init_cmds[i], &s_original_commands[i]);
        }
    }
    ++s_vendor_checks;
}

const char *esp_err_to_name(esp_err_t error) {
    return error == ESP_OK ? "ESP_OK" : "ESP_ERR_TIMEOUT";
}

esp_err_t spi_bus_initialize(int host, const spi_bus_config_t *config, int dma) {
    CHECK(host == MYBOT_AMOLED175_LCD_SPI_HOST && dma == SPI_DMA_CH_AUTO && !s_spi_open);
    CHECK(config->sclk_io_num == MYBOT_DISPLAY_PCLK);
    CHECK(config->data0_io_num == MYBOT_DISPLAY_DATA0 &&
          config->data1_io_num == MYBOT_DISPLAY_DATA1);
    CHECK(config->data2_io_num == MYBOT_DISPLAY_DATA2 &&
          config->data3_io_num == MYBOT_DISPLAY_DATA3);
    if (!strcmp(MYBOT_BOARD_NAME, "esp32-s3-touch-amoled-2.16")) {
        CHECK(config->data4_io_num == GPIO_NUM_NC && config->data5_io_num == GPIO_NUM_NC);
        CHECK(config->data6_io_num == GPIO_NUM_NC && config->data7_io_num == GPIO_NUM_NC);
    }
    CHECK(config->flags == SPICOMMON_BUSFLAG_QUAD);
    CHECK(config->max_transfer_sz ==
          (int)(MYBOT_DISPLAY_WIDTH * MYBOT_DISPLAY_TRANSFER_ROWS * sizeof(uint16_t)));
    ++s_spi_initializations;
    const esp_err_t result = step("spi_init", EVENT_SPI_NEW);
    if (result == ESP_OK) {
        s_spi_open = true;
    }
    return result;
}

esp_err_t spi_bus_free(int host) {
    CHECK(host == MYBOT_AMOLED175_LCD_SPI_HOST && s_spi_open && !s_io.open && !s_panel.open);
    const esp_err_t result = step("spi_free", EVENT_SPI_FREE);
    if (result == ESP_OK) {
        s_spi_open = false;
    }
    return result;
}

esp_err_t esp_lcd_new_panel_io_spi(esp_lcd_spi_bus_handle_t host,
                                   const esp_lcd_panel_io_spi_config_t *config,
                                   esp_lcd_panel_io_handle_t *out) {
    CHECK(host == MYBOT_AMOLED175_LCD_SPI_HOST && s_spi_open && !s_io.open);
    CHECK(config->cs_gpio_num == MYBOT_DISPLAY_CS && config->dc_gpio_num == GPIO_NUM_NC);
    CHECK(config->spi_mode == 0 && config->pclk_hz == MYBOT_AMOLED175_LCD_PIXEL_CLOCK_HZ);
    CHECK(config->lcd_cmd_bits == 32 && config->lcd_param_bits == 8 && config->flags.quad_mode);
    CHECK(config->trans_queue_depth == 2);
    const esp_err_t result = step("io_new", EVENT_IO_NEW);
    if (result == ESP_OK || s_partial_constructor) {
        s_partial_constructor = result == ESP_OK && s_partial_constructor;
        s_io.open = true;
        *out = &s_io;
    }
    return result;
}

esp_err_t esp_lcd_new_panel_co5300(esp_lcd_panel_io_handle_t io,
                                   const esp_lcd_panel_dev_config_t *config,
                                   esp_lcd_panel_handle_t *out) {
    CHECK(io == &s_io && s_io.open && !s_panel.open);
    CHECK(config->reset_gpio_num == MYBOT_DISPLAY_RESET);
    CHECK(config->rgb_ele_order == LCD_RGB_ELEMENT_ORDER_RGB && config->bits_per_pixel == 16);
    check_vendor_commands(config->vendor_config);
    const esp_err_t result = step("panel_new", EVENT_PANEL_NEW);
    if (result == ESP_OK || s_partial_constructor) {
        s_partial_constructor = result == ESP_OK && s_partial_constructor;
        s_panel = (struct host_panel){.open = true};
        *out = &s_panel;
    }
    return result;
}

esp_err_t esp_lcd_panel_io_del(esp_lcd_panel_io_handle_t io) {
    CHECK(io == &s_io && s_io.open && !s_panel.open);
    const esp_err_t result = step("io_delete", EVENT_IO_DELETE);
    if (result == ESP_OK) {
        s_io.open = false;
    }
    return result;
}

esp_err_t esp_lcd_panel_del(esp_lcd_panel_handle_t panel) {
    CHECK(panel == &s_panel && s_panel.open && s_io.open);
    const esp_err_t result = step("panel_delete", EVENT_PANEL_DELETE);
    if (result == ESP_OK) {
        s_panel = (struct host_panel){0};
    }
    return result;
}

esp_err_t esp_lcd_panel_reset(esp_lcd_panel_handle_t panel) {
    CHECK(panel == &s_panel && s_panel.open);
    return step("panel_reset", EVENT_RESET);
}
esp_err_t esp_lcd_panel_init(esp_lcd_panel_handle_t panel) {
    CHECK(panel == &s_panel && s_panel.open);
    const esp_err_t result = step("panel_init", EVENT_INIT);
    if (result == ESP_OK) {
        s_panel.initialized = true;
    }
    return result;
}
esp_err_t esp_lcd_panel_set_gap(esp_lcd_panel_handle_t panel, int x, int y) {
    CHECK(panel == &s_panel && s_panel.open);
    CHECK(x == MYBOT_DISPLAY_OFFSET_X && y == MYBOT_DISPLAY_OFFSET_Y);
    return step("panel_gap", EVENT_GAP);
}
esp_err_t esp_lcd_panel_invert_color(esp_lcd_panel_handle_t panel, bool invert) {
    CHECK(panel == &s_panel && s_panel.open && invert == MYBOT_DISPLAY_INVERT_COLOR);
    return step("panel_invert", EVENT_INVERT);
}
esp_err_t esp_lcd_panel_swap_xy(esp_lcd_panel_handle_t panel, bool swap) {
    CHECK(panel == &s_panel && s_panel.open && swap == MYBOT_DISPLAY_SWAP_XY);
    return step("panel_swap", EVENT_SWAP);
}
esp_err_t esp_lcd_panel_mirror(esp_lcd_panel_handle_t panel, bool x, bool y) {
    CHECK(panel == &s_panel && s_panel.open);
    CHECK(x == MYBOT_DISPLAY_MIRROR_X && y == MYBOT_DISPLAY_MIRROR_Y);
    return step("panel_mirror", EVENT_MIRROR);
}
esp_err_t esp_lcd_panel_disp_on_off(esp_lcd_panel_handle_t panel, bool on) {
    CHECK(panel == &s_panel && s_panel.open && (!on || s_panel.initialized));
    const esp_err_t result = step(on ? "display_on" : "display_off", on ? EVENT_ON : EVENT_OFF);
    if (result == ESP_OK) {
        s_panel.on = on;
    }
    return result;
}
esp_err_t esp_lcd_panel_co5300_set_brightness(esp_lcd_panel_handle_t panel, unsigned int percent) {
    CHECK(panel == &s_panel && s_panel.open && percent <= 100);
    CHECK(!percent || (s_panel.initialized && s_panel.on && percent == 60));
    const esp_err_t result = step(percent ? "brightness_on" : "brightness_off",
                                  percent ? EVENT_BRIGHTNESS_ON : EVENT_BRIGHTNESS_OFF);
    if (result == ESP_OK) {
        s_panel.brightness = percent;
    }
    return result;
}

static void assert_released(const mybot_display_panel_t *lcd) {
    CHECK(!lcd->panel && !lcd->io && !lcd->spi_ready && !lcd->ready);
    CHECK(lcd->x_alignment == 0 && lcd->y_alignment == 0);
    CHECK(!s_panel.open && !s_io.open && !s_spi_open);
}

static void test_failures_and_partial_handles(void) {
    mybot_display_panel_t lcd = {0};
    CHECK(mybot_display_panel_open(NULL) < 0 && mybot_display_panel_close(NULL) < 0);
    const char *failures[] = {"spi_init",     "io_new",     "panel_new",    "panel_gap",
                              "panel_reset",  "panel_init", "panel_invert", "panel_swap",
                              "panel_mirror", "display_on", "brightness_on"};
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        s_event_count = 0;
        s_failure = failures[i];
        CHECK(mybot_display_panel_open(&lcd) < 0 && s_failure == NULL);
        assert_released(&lcd);
        CHECK(mybot_display_panel_open(&lcd) == 0);
        CHECK(mybot_display_panel_close(&lcd) == 0);
        assert_released(&lcd);
    }
    const char *constructors[] = {"io_new", "panel_new"};
    for (size_t i = 0; i < sizeof(constructors) / sizeof(constructors[0]); ++i) {
        s_event_count = 0;
        s_failure = constructors[i];
        s_partial_constructor = true;
        CHECK(mybot_display_panel_open(&lcd) < 0 && !s_partial_constructor);
        assert_released(&lcd);
    }
    s_event_count = 0;
    s_failure = "panel_init";
    s_cleanup_failure = "panel_delete";
    CHECK(mybot_display_panel_open(&lcd) < 0);
    CHECK(lcd.panel == &s_panel && lcd.io == &s_io && lcd.spi_ready);
    const unsigned int inits = s_spi_initializations;
    s_failure = "panel_delete";
    CHECK(mybot_display_panel_open(&lcd) < 0 && s_failure == NULL);
    CHECK(s_spi_initializations == inits && lcd.panel == &s_panel && lcd.io == &s_io);
    CHECK(mybot_display_panel_open(&lcd) == 0);
    CHECK(mybot_display_panel_close(&lcd) == 0);
    assert_released(&lcd);
}

static void test_cleanup_retry_and_repeated_open(void) {
    mybot_display_panel_t lcd = {0};
    const char *failures[] = {"panel_delete", "io_delete", "spi_free"};
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        s_event_count = 0;
        CHECK(mybot_display_panel_open(&lcd) == 0);
        s_failure = failures[i];
        CHECK(mybot_display_panel_close(&lcd) < 0 && s_failure == NULL);
        CHECK(lcd.spi_ready && s_spi_open);
        CHECK(mybot_display_panel_close(&lcd) == 0);
        CHECK(mybot_display_panel_close(&lcd) == 0);
        assert_released(&lcd);
    }
    const char *best_effort[] = {"brightness_off", "display_off"};
    for (size_t i = 0; i < sizeof(best_effort) / sizeof(best_effort[0]); ++i) {
        s_event_count = 0;
        CHECK(mybot_display_panel_open(&lcd) == 0);
        s_failure = best_effort[i];
        CHECK(mybot_display_panel_close(&lcd) == 0 && s_failure == NULL);
        assert_released(&lcd);
    }
    for (unsigned int cycle = 0; cycle < 3; ++cycle) {
        s_event_count = 0;
        CHECK(mybot_display_panel_open(&lcd) == 0);
        CHECK(lcd.ready && lcd.x_alignment == 2 && lcd.y_alignment == 2);
        CHECK(s_panel.brightness == 60);
        const unsigned int inits = s_spi_initializations;
        CHECK(mybot_display_panel_open(&lcd) < 0 && s_spi_initializations == inits);
        CHECK(event_position(EVENT_INIT) < event_position(EVENT_ON));
        CHECK(event_position(EVENT_ON) < event_position(EVENT_BRIGHTNESS_ON));
        s_event_count = 0;
        CHECK(mybot_display_panel_close(&lcd) == 0);
        CHECK(event_position(EVENT_BRIGHTNESS_OFF) < event_position(EVENT_OFF));
        CHECK(event_position(EVENT_OFF) < event_position(EVENT_PANEL_DELETE));
        CHECK(event_position(EVENT_PANEL_DELETE) < event_position(EVENT_IO_DELETE));
        CHECK(event_position(EVENT_IO_DELETE) < event_position(EVENT_SPI_FREE));
        assert_released(&lcd);
    }
}

int main(void) {
    test_failures_and_partial_handles();
    test_cleanup_retry_and_repeated_open();
    CHECK(s_vendor_checks > 0);
    printf("CO5300 host configuration and fault-injection tests passed: %s\n", MYBOT_BOARD_NAME);
    return 0;
}
