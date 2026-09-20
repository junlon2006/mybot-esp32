/* SPDX-License-Identifier: MIT */
/* Generic LVGL/ST7789 adapter for board profiles with a SPI RGB565 panel. */
#include "board_config.h"
#include "cores3_lvgl_view.h"

#include <mybot/platform/mybot_lcd.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdint.h>

#ifndef MYBOT_DISPLAY_SPI_HOST
#define MYBOT_DISPLAY_SPI_HOST SPI3_HOST
#endif
#ifndef MYBOT_DISPLAY_SPI_MODE
#define MYBOT_DISPLAY_SPI_MODE 0
#endif
#ifndef MYBOT_DISPLAY_PIXEL_CLOCK_HZ
#define MYBOT_DISPLAY_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#endif
#ifndef MYBOT_DISPLAY_INVERT_COLOR
#define MYBOT_DISPLAY_INVERT_COLOR true
#endif
#ifndef MYBOT_DISPLAY_OFFSET_X
#define MYBOT_DISPLAY_OFFSET_X 0
#endif
#ifndef MYBOT_DISPLAY_OFFSET_Y
#define MYBOT_DISPLAY_OFFSET_Y 0
#endif
#ifndef MYBOT_DISPLAY_MIRROR_X
#define MYBOT_DISPLAY_MIRROR_X false
#endif
#ifndef MYBOT_DISPLAY_MIRROR_Y
#define MYBOT_DISPLAY_MIRROR_Y false
#endif
#ifndef MYBOT_DISPLAY_SWAP_XY
#define MYBOT_DISPLAY_SWAP_XY false
#endif
#ifndef MYBOT_DISPLAY_RGB_ORDER
#define MYBOT_DISPLAY_RGB_ORDER LCD_RGB_ELEMENT_ORDER_RGB
#endif
#ifndef MYBOT_LVGL_OPS_NAME
#define MYBOT_LVGL_OPS_NAME mybot_st7789_lvgl_lcd_ops
#endif

#define TAG "lvgl_st7789"
#define TRANSFER_ROWS 16
#define LOCK_TIMEOUT_MS 1000
#define UI_STACK_BYTES 7168

namespace {

struct Context {
    esp_lcd_panel_io_handle_t io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;
    lv_display_t *display = nullptr;
    SemaphoreHandle_t lifecycle = nullptr;
    StaticSemaphore_t lifecycle_storage{};
    volatile uint32_t flushing = 0;
    unsigned users = 0;
    bool spi_ready = false;
    bool port_started = false;
    bool accepting = false;
};

Context s_context;
portMUX_TYPE s_lifecycle_init_lock = portMUX_INITIALIZER_UNLOCKED;

bool lock() {
    portENTER_CRITICAL(&s_lifecycle_init_lock);
    if (!s_context.lifecycle) {
        s_context.lifecycle = xSemaphoreCreateMutexStatic(&s_context.lifecycle_storage);
    }
    SemaphoreHandle_t lifecycle = s_context.lifecycle;
    portEXIT_CRITICAL(&s_lifecycle_init_lock);
    return lifecycle && xSemaphoreTake(lifecycle, pdMS_TO_TICKS(LOCK_TIMEOUT_MS)) == pdTRUE;
}

void unlock() {
    xSemaphoreGive(s_context.lifecycle);
}

bool transfer_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *user) {
    auto *context = static_cast<Context *>(user);
    lvgl_port_flush_ready(context->display);
    __atomic_sub_fetch(&context->flushing, 1, __ATOMIC_RELEASE);
    return false;
}

void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
    __atomic_add_fetch(&s_context.flushing, 1, __ATOMIC_ACQ_REL);
    lv_draw_sw_rgb565_swap(pixels, lv_area_get_size(area));
    const esp_err_t result = esp_lcd_panel_draw_bitmap(s_context.panel, area->x1, area->y1,
                                                       area->x2 + 1, area->y2 + 1, pixels);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "event=lcd action=flush result=error error=%s", esp_err_to_name(result));
        lvgl_port_flush_ready(display);
        __atomic_sub_fetch(&s_context.flushing, 1, __ATOMIC_RELEASE);
    }
}

int close_panel() {
    if (s_context.panel && esp_lcd_panel_del(s_context.panel) != ESP_OK) {
        return -1;
    }
    s_context.panel = nullptr;
    if (s_context.io && esp_lcd_panel_io_del(s_context.io) != ESP_OK) {
        return -1;
    }
    s_context.io = nullptr;
    if (s_context.spi_ready && spi_bus_free(MYBOT_DISPLAY_SPI_HOST) != ESP_OK) {
        return -1;
    }
    s_context.spi_ready = false;
    gpio_set_level(MYBOT_DISPLAY_BACKLIGHT, 0);
    return 0;
}

int open_panel() {
    gpio_config_t backlight = {
        .pin_bit_mask = 1ULL << MYBOT_DISPLAY_BACKLIGHT,
        .mode = GPIO_MODE_OUTPUT,
    };
    if (gpio_config(&backlight) != ESP_OK) {
        return -1;
    }
    gpio_set_level(MYBOT_DISPLAY_BACKLIGHT, 0);
    const spi_bus_config_t bus = {
        .mosi_io_num = MYBOT_DISPLAY_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = MYBOT_DISPLAY_SCLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = MYBOT_DISPLAY_WIDTH * TRANSFER_ROWS * sizeof(uint16_t),
    };
    if (spi_bus_initialize(MYBOT_DISPLAY_SPI_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
        return -1;
    }
    s_context.spi_ready = true;
    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = MYBOT_DISPLAY_CS,
        .dc_gpio_num = MYBOT_DISPLAY_DC,
        .spi_mode = MYBOT_DISPLAY_SPI_MODE,
        .pclk_hz = MYBOT_DISPLAY_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 2,
        .on_color_trans_done = transfer_done,
        .user_ctx = &s_context,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    if (esp_lcd_new_panel_io_spi(MYBOT_DISPLAY_SPI_HOST, &io_config, &s_context.io) != ESP_OK) {
        close_panel();
        return -1;
    }
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = MYBOT_DISPLAY_RESET,
        .rgb_ele_order = MYBOT_DISPLAY_RGB_ORDER,
        .bits_per_pixel = 16,
    };
    esp_err_t result = esp_lcd_new_panel_st7789(s_context.io, &panel_config, &s_context.panel);
    if (result == ESP_OK)
        result = esp_lcd_panel_reset(s_context.panel);
    if (result == ESP_OK)
        result = esp_lcd_panel_init(s_context.panel);
    if (result == ESP_OK)
        result =
            esp_lcd_panel_set_gap(s_context.panel, MYBOT_DISPLAY_OFFSET_X, MYBOT_DISPLAY_OFFSET_Y);
    if (result == ESP_OK)
        result = esp_lcd_panel_invert_color(s_context.panel, MYBOT_DISPLAY_INVERT_COLOR);
    if (result == ESP_OK)
        result = esp_lcd_panel_swap_xy(s_context.panel, MYBOT_DISPLAY_SWAP_XY);
    if (result == ESP_OK)
        result =
            esp_lcd_panel_mirror(s_context.panel, MYBOT_DISPLAY_MIRROR_X, MYBOT_DISPLAY_MIRROR_Y);
    if (result == ESP_OK)
        result = esp_lcd_panel_disp_on_off(s_context.panel, true);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "event=lcd action=initialize component=st7789 result=error error=%s",
                 esp_err_to_name(result));
        close_panel();
        return -1;
    }
    gpio_set_level(MYBOT_DISPLAY_BACKLIGHT, 1);
    return 0;
}

int initialize(void **out) {
    if (!out || !lock())
        return -1;
    *out = nullptr;
    if (s_context.users) {
        ++s_context.users;
        *out = &s_context;
        unlock();
        return 0;
    }
    if (open_panel() < 0) {
        unlock();
        return -1;
    }
    lvgl_port_cfg_t port = ESP_LVGL_PORT_INIT_CONFIG();
    port.task_priority = 1;
    port.task_affinity = 1;
    port.task_stack = UI_STACK_BYTES;
    port.timer_period_ms = 5;
    if (lvgl_port_init(&port) != ESP_OK) {
        close_panel();
        unlock();
        return -1;
    }
    s_context.port_started = true;
    if (!lvgl_port_lock(LOCK_TIMEOUT_MS)) {
        (void)lvgl_port_deinit();
        s_context.port_started = false;
        close_panel();
        unlock();
        return -1;
    }
    lvgl_port_display_cfg_t config{};
    config.io_handle = s_context.io;
    config.panel_handle = s_context.panel;
    config.buffer_size = MYBOT_DISPLAY_WIDTH * TRANSFER_ROWS;
    config.hres = MYBOT_DISPLAY_WIDTH;
    config.vres = MYBOT_DISPLAY_HEIGHT;
    config.color_format = LV_COLOR_FORMAT_RGB565;
    config.flags.buff_dma = true;
    config.flags.swap_bytes = false;
    s_context.display = lvgl_port_add_disp(&config);
    if (!s_context.display ||
        mybot_cores3_lvgl_view_create_sized(s_context.display, MYBOT_DISPLAY_WIDTH,
                                            MYBOT_DISPLAY_HEIGHT) < 0) {
        if (s_context.display)
            (void)lvgl_port_remove_disp(s_context.display);
        s_context.display = nullptr;
        lvgl_port_unlock();
        lvgl_port_deinit();
        s_context.port_started = false;
        close_panel();
        unlock();
        return -1;
    }
    lv_display_set_flush_cb(s_context.display, flush);
    s_context.accepting = true;
    s_context.users = 1;
    lvgl_port_unlock();
    *out = &s_context;
    ESP_LOGI(
        TAG, "event=lcd action=initialize backend=lvgl width=%d height=%d offset=%d,%d result=ok",
        MYBOT_DISPLAY_WIDTH, MYBOT_DISPLAY_HEIGHT, MYBOT_DISPLAY_OFFSET_X, MYBOT_DISPLAY_OFFSET_Y);
    unlock();
    return 0;
}

int render(void *opaque, const mybot_lcd_content_t *content) {
    if (opaque != &s_context || !content || !lock())
        return -1;
    int result = -1;
    if (s_context.accepting && s_context.display && lvgl_port_lock(LOCK_TIMEOUT_MS)) {
        mybot_cores3_lvgl_view_update(content);
        const esp_err_t wake_result = lvgl_port_task_wake(LVGL_PORT_EVENT_USER, nullptr);
        lvgl_port_unlock();
        result = wake_result == ESP_OK ? 0 : -1;
    }
    unlock();
    return result;
}

void destroy(void *opaque) {
    if (opaque != &s_context || !lock())
        return;
    if (s_context.users > 1) {
        --s_context.users;
        unlock();
        return;
    }
    s_context.accepting = false;
    if (s_context.display && lvgl_port_lock(LOCK_TIMEOUT_MS)) {
        lv_display_delete_refr_timer(s_context.display);
        const int64_t deadline = esp_timer_get_time() + LOCK_TIMEOUT_MS * 1000;
        while (__atomic_load_n(&s_context.flushing, __ATOMIC_ACQUIRE) &&
               esp_timer_get_time() < deadline) {
            vTaskDelay(1);
        }
        if (!__atomic_load_n(&s_context.flushing, __ATOMIC_ACQUIRE)) {
            mybot_cores3_lvgl_view_destroy();
            (void)lvgl_port_remove_disp(s_context.display);
            s_context.display = nullptr;
        }
        lvgl_port_unlock();
    }
    if (s_context.display) {
        unlock();
        return;
    }
    if (close_panel() < 0) {
        ESP_LOGE(TAG, "event=lcd action=destroy result=deferred reason=panel_cleanup");
        unlock();
        return;
    }
    if (s_context.port_started) {
        if (lvgl_port_deinit() != ESP_OK) {
            ESP_LOGE(TAG, "event=lcd action=destroy result=deferred reason=lvgl_deinit");
            unlock();
            return;
        }
        s_context.port_started = false;
    }
    s_context.users = 0;
    __atomic_store_n(&s_context.flushing, 0, __ATOMIC_RELEASE);
    unlock();
}

const mybot_lcd_ops_t s_ops = {initialize, render, destroy};
} // namespace

extern "C" const mybot_lcd_ops_t *MYBOT_LVGL_OPS_NAME(void) {
    return &s_ops;
}
