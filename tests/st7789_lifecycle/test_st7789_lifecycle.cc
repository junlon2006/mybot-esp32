/* SPDX-License-Identifier: MIT */
#include "host_platform.h"

#include "board_config.h"
#include "display/display_panel.h"
#include "display/lvgl_view.h"

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

struct host_panel_io {
    bool open = false;
    esp_lcd_panel_io_color_trans_done_cb_t callback = nullptr;
    void *user = nullptr;
};
struct host_panel {
    bool open = false;
    bool initialized = false;
    bool on = false;
};
struct host_display {
    bool open = false;
    bool refresh = false;
    lv_display_flush_cb_t flush = nullptr;
    uint32_t event_count = 0;
};
struct host_timer {
    bool open = false;
    lv_timer_cb_t callback = nullptr;
};

static host_panel_io io;
static host_panel panel;
static host_display display;
static host_timer timer;
static bool spi_open;
static bool port_open;
static bool port_locked;
static bool view_open;
static unsigned int backlight;
static unsigned int pending_transfers;
static unsigned int ready_calls;
static unsigned int display_removes;
static unsigned int spi_initializations;
static unsigned int view_updates;
static int64_t time_us;
static bool partial_constructor;
static bool complete_on_delay;
static std::string failed_step;
static std::string failed_cleanup;
static std::vector<std::string> events;
static mybot_lcd_content_t applied;

static esp_err_t step(const char *name) {
    events.emplace_back(name);
    if (failed_step == name) {
        failed_step.clear();
        return ESP_ERR_TIMEOUT;
    }
    if (failed_cleanup == name) {
        failed_cleanup.clear();
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
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

static void complete_transfer() {
    CHECK(io.open && io.callback && pending_transfers > 0 && display.open);
    events.emplace_back("callback");
    --pending_transfers;
    CHECK(!io.callback(&io, nullptr, io.user));
}

extern "C" {
const char *esp_err_to_name(esp_err_t error) {
    return error == ESP_OK ? "ESP_OK" : "ESP_ERR_TIMEOUT";
}

int mybot_board_set_display_backlight(unsigned int percent) {
    CHECK(percent <= 100);
    const esp_err_t result = step(percent ? "backlight_on" : "backlight_off");
    if (result != ESP_OK) {
        return -1;
    }
    if (percent) {
        CHECK(panel.open && panel.initialized && panel.on);
    }
    backlight = percent;
    return 0;
}

esp_err_t spi_bus_initialize(int host, const spi_bus_config_t *config, int dma) {
    CHECK(host == MYBOT_DISPLAY_SPI_HOST && dma == SPI_DMA_CH_AUTO && !spi_open);
    CHECK(config->mosi_io_num == MYBOT_DISPLAY_MOSI);
    CHECK(config->sclk_io_num == MYBOT_DISPLAY_SCLK);
    CHECK(config->miso_io_num == GPIO_NUM_NC);
    CHECK(config->max_transfer_sz ==
          static_cast<int>(MYBOT_DISPLAY_WIDTH * MYBOT_DISPLAY_TRANSFER_ROWS * sizeof(uint16_t)));
    ++spi_initializations;
    const esp_err_t result = step("spi_init");
    if (result == ESP_OK) {
        spi_open = true;
    }
    return result;
}

esp_err_t spi_bus_free(int host) {
    CHECK(host == MYBOT_DISPLAY_SPI_HOST && spi_open && !io.open && !panel.open);
    const esp_err_t result = step("spi_free");
    if (result == ESP_OK) {
        spi_open = false;
    }
    return result;
}

esp_err_t esp_lcd_new_panel_io_spi(esp_lcd_spi_bus_handle_t host,
                                   const esp_lcd_panel_io_spi_config_t *config,
                                   esp_lcd_panel_io_handle_t *out) {
    CHECK(host == MYBOT_DISPLAY_SPI_HOST && spi_open && !io.open);
    CHECK(config->cs_gpio_num == MYBOT_DISPLAY_CS);
    CHECK(config->dc_gpio_num == MYBOT_DISPLAY_DC);
    CHECK(config->spi_mode == MYBOT_DISPLAY_SPI_MODE);
    CHECK(config->pclk_hz == MYBOT_DISPLAY_PIXEL_CLOCK_HZ);
    CHECK(config->lcd_cmd_bits == 8 && config->lcd_param_bits == 8);
    const esp_err_t result = step("io_new");
    if (result == ESP_OK || partial_constructor) {
        if (result != ESP_OK) {
            partial_constructor = false;
        }
        io = {};
        io.open = true;
        *out = &io;
    }
    return result;
}

esp_err_t esp_lcd_new_panel_st7789(esp_lcd_panel_io_handle_t handle,
                                   const esp_lcd_panel_dev_config_t *config,
                                   esp_lcd_panel_handle_t *out) {
    CHECK(handle == &io && io.open && !panel.open);
    CHECK(config->reset_gpio_num == MYBOT_DISPLAY_RESET);
    CHECK(config->rgb_ele_order == MYBOT_DISPLAY_RGB_ORDER && config->bits_per_pixel == 16);
    const esp_err_t result = step("panel_new");
    if (result == ESP_OK || partial_constructor) {
        if (result != ESP_OK) {
            partial_constructor = false;
        }
        panel = {};
        panel.open = true;
        *out = &panel;
    }
    return result;
}

esp_err_t esp_lcd_panel_io_del(esp_lcd_panel_io_handle_t handle) {
    CHECK(handle == &io && io.open && !panel.open && pending_transfers == 0);
    const esp_err_t result = step("io_delete");
    if (result == ESP_OK) {
        io = {};
    }
    return result;
}

esp_err_t esp_lcd_panel_del(esp_lcd_panel_handle_t handle) {
    CHECK(handle == &panel && panel.open && io.open && pending_transfers == 0);
    CHECK(backlight == 0);
    const esp_err_t result = step("panel_delete");
    if (result == ESP_OK) {
        panel = {};
    }
    return result;
}

esp_err_t esp_lcd_panel_reset(esp_lcd_panel_handle_t handle) {
    CHECK(handle == &panel && panel.open);
    return step("panel_reset");
}
esp_err_t esp_lcd_panel_init(esp_lcd_panel_handle_t handle) {
    CHECK(handle == &panel && panel.open);
    const esp_err_t result = step("panel_init");
    if (result == ESP_OK) {
        panel.initialized = true;
    }
    return result;
}
esp_err_t esp_lcd_panel_set_gap(esp_lcd_panel_handle_t handle, int x, int y) {
    CHECK(handle == &panel && panel.open);
    CHECK(x == MYBOT_DISPLAY_OFFSET_X && y == MYBOT_DISPLAY_OFFSET_Y);
    return step("panel_gap");
}
esp_err_t esp_lcd_panel_invert_color(esp_lcd_panel_handle_t handle, bool invert) {
    CHECK(handle == &panel && panel.open && invert == MYBOT_DISPLAY_INVERT_COLOR);
    return step("panel_invert");
}
esp_err_t esp_lcd_panel_swap_xy(esp_lcd_panel_handle_t handle, bool swap) {
    CHECK(handle == &panel && panel.open && swap == MYBOT_DISPLAY_SWAP_XY);
    return step("panel_swap");
}
esp_err_t esp_lcd_panel_mirror(esp_lcd_panel_handle_t handle, bool x, bool y) {
    CHECK(handle == &panel && panel.open);
    CHECK(x == MYBOT_DISPLAY_MIRROR_X && y == MYBOT_DISPLAY_MIRROR_Y);
    return step("panel_mirror");
}
esp_err_t esp_lcd_panel_disp_on_off(esp_lcd_panel_handle_t handle, bool on) {
    CHECK(handle == &panel && panel.open);
    CHECK(!on || panel.initialized);
    const esp_err_t result = step(on ? "display_on" : "display_off");
    if (result == ESP_OK) {
        panel.on = on;
    }
    return result;
}
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t handle, int x1, int y1, int x2, int y2,
                                    const void *pixels) {
    CHECK(handle == &panel && panel.open && panel.on && pixels);
    CHECK(x1 == 0 && y1 == 0 && x2 == 2 && y2 == 1);
    const esp_err_t result = step("draw");
    if (result == ESP_OK) {
        ++pending_transfers;
    }
    return result;
}
esp_err_t esp_lcd_panel_io_register_event_callbacks(esp_lcd_panel_io_handle_t handle,
                                                    const esp_lcd_panel_io_callbacks_t *callbacks,
                                                    void *user) {
    CHECK(handle == &io && io.open && callbacks->on_color_trans_done);
    const esp_err_t result = step("register_callback");
    if (result == ESP_OK) {
        io.callback = callbacks->on_color_trans_done;
        io.user = user;
    }
    return result;
}

SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage) {
    storage->taken = false;
    return storage;
}
int xSemaphoreTake(SemaphoreHandle_t semaphore, unsigned int ticks) {
    CHECK(semaphore && !semaphore->taken && ticks == 1000);
    if (step("lifecycle_lock") != ESP_OK) {
        return 0;
    }
    semaphore->taken = true;
    return pdTRUE;
}
int xSemaphoreGive(SemaphoreHandle_t semaphore) {
    CHECK(semaphore && semaphore->taken);
    semaphore->taken = false;
    return pdTRUE;
}
void vTaskDelay(unsigned int ticks) {
    time_us += ticks * 1000;
    if (complete_on_delay) {
        complete_on_delay = false;
        complete_transfer();
    }
}
int64_t esp_timer_get_time(void) {
    return time_us;
}

lv_area_t *lv_event_get_invalidated_area(lv_event_t *event) {
    return event->area;
}
int lv_area_get_size(const lv_area_t *area) {
    return (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
}
void lv_draw_sw_rgb565_swap(void *pixels, int size) {
    auto *bytes = static_cast<uint8_t *>(pixels);
    for (int i = 0; i < size; ++i) {
        const uint8_t first = bytes[2 * i];
        bytes[2 * i] = bytes[2 * i + 1];
        bytes[2 * i + 1] = first;
    }
}
uint32_t lv_display_get_event_count(lv_display_t *handle) {
    CHECK(handle == &display && display.open);
    return display.event_count;
}
void lv_display_add_event_cb(lv_display_t *handle, void (*callback)(lv_event_t *), int code,
                             void *user) {
    CHECK(handle == &display && display.open && callback);
    CHECK(code == LV_EVENT_INVALIDATE_AREA && !user);
    ++display.event_count;
}
void lv_display_set_flush_cb(lv_display_t *handle, lv_display_flush_cb_t callback) {
    CHECK(handle == &display && display.open && callback);
    display.flush = callback;
}
void lv_display_delete_refr_timer(lv_display_t *handle) {
    CHECK(handle == &display && display.open && port_locked);
    events.emplace_back("refresh_stop");
    display.refresh = false;
}
lv_timer_t *lv_timer_create(lv_timer_cb_t callback, unsigned int period, void *user) {
    CHECK(callback && period == 50 && !user && !timer.open && port_locked);
    if (step("timer_new") != ESP_OK) {
        return nullptr;
    }
    timer.open = true;
    timer.callback = callback;
    return &timer;
}
void lv_timer_delete(lv_timer_t *handle) {
    CHECK(handle == &timer && timer.open && port_locked);
    events.emplace_back("timer_delete");
    timer = {};
}

esp_err_t lvgl_port_init(const lvgl_port_cfg_t *config) {
    CHECK(!port_open && config->task_affinity == 1 && config->task_priority == 1);
    /* A failed worker startup may still need to be joined. */
    port_open = true;
    return step("port_init");
}
esp_err_t lvgl_port_deinit(void) {
    CHECK(port_open && !display.open && !io.open && !panel.open && !spi_open);
    const esp_err_t result = step("port_deinit");
    if (result == ESP_OK) {
        port_open = false;
    }
    return result;
}
bool lvgl_port_lock(unsigned int timeout) {
    CHECK(port_open && !port_locked && timeout == 1000);
    if (step("port_lock") != ESP_OK) {
        return false;
    }
    port_locked = true;
    return true;
}
void lvgl_port_unlock(void) {
    CHECK(port_locked);
    port_locked = false;
}
lv_display_t *lvgl_port_add_disp(const lvgl_port_display_cfg_t *config) {
    CHECK(port_locked && !display.open);
    CHECK(config->io_handle == &io && config->panel_handle == &panel);
    CHECK(config->hres == MYBOT_DISPLAY_WIDTH && config->vres == MYBOT_DISPLAY_HEIGHT);
    CHECK(config->buffer_size == MYBOT_DISPLAY_WIDTH * MYBOT_DISPLAY_TRANSFER_ROWS);
    CHECK(config->flags.buff_dma && !config->flags.swap_bytes);
    if (step("display_new") != ESP_OK) {
        return nullptr;
    }
    display = {};
    display.open = true;
    display.refresh = true;
    return &display;
}
esp_err_t lvgl_port_remove_disp(lv_display_t *handle) {
    CHECK(handle == &display && display.open && !display.refresh && !timer.open && !view_open);
    CHECK(!io.open && !panel.open && !spi_open && pending_transfers == 0);
    const esp_err_t result = step("display_remove");
    if (result == ESP_OK) {
        display = {};
        ++display_removes;
    }
    return result;
}
void lvgl_port_flush_ready(lv_display_t *handle) {
    CHECK(handle == &display && display.open);
    events.emplace_back("flush_ready");
    ++ready_calls;
}
esp_err_t lvgl_port_task_wake(int event, void *user) {
    CHECK(port_open && event == LVGL_PORT_EVENT_USER && !user);
    return step("wake");
}
int mybot_lvgl_view_create_sized(lv_display_t *handle, int width, int height) {
    CHECK(handle == &display && display.open && port_locked && !view_open);
    CHECK(width == MYBOT_DISPLAY_WIDTH && height == MYBOT_DISPLAY_HEIGHT);
    if (step("view_new") != ESP_OK) {
        return -1;
    }
    view_open = true;
    return 0;
}
void mybot_lvgl_view_destroy(void) {
    CHECK(port_locked && pending_transfers == 0);
    events.emplace_back("view_delete");
    view_open = false;
}
void mybot_lvgl_view_update(const mybot_lcd_content_t *content) {
    CHECK(view_open && content);
    applied = *content;
    ++view_updates;
}
const mybot_lcd_ops_t *mybot_shared_lvgl_ops(void);
}

static void assert_panel_released(const mybot_display_panel_t &lcd) {
    CHECK(!lcd.ready && !lcd.backlight_ready && !lcd.spi_ready && !lcd.io && !lcd.panel);
    CHECK(!spi_open && !io.open && !panel.open && backlight == 0);
}

static void assert_all_released() {
    CHECK(!spi_open && !io.open && !panel.open && !display.open);
    CHECK(!port_open && !port_locked && !timer.open && !view_open);
    CHECK(pending_transfers == 0 && backlight == 0);
}

static void test_panel_initialization_failures() {
    mybot_display_panel_t lcd{};
    CHECK(mybot_display_panel_open(nullptr) < 0);
    CHECK(mybot_display_panel_close(nullptr) < 0);
    const char *failures[] = {"backlight_off", "spi_init",     "io_new",     "panel_new",
                              "panel_reset",   "panel_init",   "panel_gap",  "panel_invert",
                              "panel_swap",    "panel_mirror", "display_on", "backlight_on"};
    for (const char *failure : failures) {
        events.clear();
        failed_step = failure;
        CHECK(mybot_display_panel_open(&lcd) < 0);
        CHECK(failed_step.empty());
        CHECK(mybot_display_panel_close(&lcd) == 0);
        assert_panel_released(lcd);
    }
    for (const char *failure : {"io_new", "panel_new"}) {
        failed_step = failure;
        partial_constructor = true;
        CHECK(mybot_display_panel_open(&lcd) < 0);
        CHECK(!partial_constructor);
        assert_panel_released(lcd);
    }
    failed_step = "panel_init";
    failed_cleanup = "panel_delete";
    CHECK(mybot_display_panel_open(&lcd) < 0);
    CHECK(lcd.panel == &panel && lcd.io == &io && lcd.spi_ready);
    CHECK(mybot_display_panel_close(&lcd) == 0);
    assert_panel_released(lcd);
}

static void test_panel_cleanup_retry_and_ordering() {
    mybot_display_panel_t lcd{};
    for (const char *failure : {"backlight_off", "panel_delete", "io_delete", "spi_free"}) {
        CHECK(mybot_display_panel_open(&lcd) == 0);
        CHECK(lcd.ready && backlight > 0);
        failed_step = failure;
        CHECK(mybot_display_panel_close(&lcd) < 0);
        CHECK(failed_step.empty());
        CHECK(spi_open);
        CHECK(mybot_display_panel_close(&lcd) == 0);
        assert_panel_released(lcd);
        CHECK(mybot_display_panel_close(&lcd) == 0);
    }
    for (unsigned int cycle = 0; cycle < 3; ++cycle) {
        events.clear();
        CHECK(mybot_display_panel_open(&lcd) == 0);
        const unsigned int inits = spi_initializations;
        CHECK(mybot_display_panel_open(&lcd) < 0);
        CHECK(spi_initializations == inits);
        CHECK(event_position("panel_init") < event_position("display_on"));
        CHECK(event_position("display_on") < event_position("backlight_on"));
        events.clear();
        CHECK(mybot_display_panel_close(&lcd) == 0);
        CHECK(event_position("backlight_off") < event_position("panel_delete"));
        CHECK(event_position("panel_delete") < event_position("io_delete"));
        CHECK(event_position("io_delete") < event_position("spi_free"));
        assert_panel_released(lcd);
    }
}

static void test_adapter_initialization_failures() {
    const mybot_lcd_ops_t *ops = mybot_shared_lvgl_ops();
    CHECK(ops->init(nullptr) < 0);
    for (const char *failure :
         {"port_init", "port_lock", "display_new", "register_callback", "view_new", "timer_new"}) {
        void *context = reinterpret_cast<void *>(1);
        failed_step = failure;
        CHECK(ops->init(&context) < 0);
        CHECK(!context && failed_step.empty());
        assert_all_released();
    }
}

static void submit_flush() {
    CHECK(display.open && display.refresh && display.flush);
    const lv_area_t area = {.x1 = 0, .y1 = 0, .x2 = 1, .y2 = 0};
    uint8_t pixels[] = {0x34, 0x12, 0xcd, 0xab};
    display.flush(&display, &area, pixels);
    CHECK(pixels[0] == 0x12 && pixels[1] == 0x34 && pixels[2] == 0xab && pixels[3] == 0xcd);
}

static void test_adapter_references_and_callback_ordering() {
    const mybot_lcd_ops_t *ops = mybot_shared_lvgl_ops();
    for (unsigned int cycle = 0; cycle < 3; ++cycle) {
        void *owner = nullptr;
        void *sdk = nullptr;
        CHECK(ops->init(&owner) == 0);
        const unsigned int inits = spi_initializations;
        CHECK(ops->init(&sdk) == 0 && sdk == owner);
        CHECK(spi_initializations == inits);
        ops->destroy(sdk);
        CHECK(spi_open && display.open && port_open);

        mybot_lcd_content_t content{};
        content.screen = MYBOT_LCD_SCREEN_PAIR_CODE;
        std::strcpy(content.pair_code, "123456");
        CHECK(ops->render(owner, &content) == 0);
        std::strcpy(content.pair_code, "xxxxxx");
        const unsigned int updates = view_updates;
        timer.callback(&timer);
        CHECK(view_updates == updates + 1 && !std::strcmp(applied.pair_code, "123456"));

        events.clear();
        const unsigned int ready = ready_calls;
        submit_flush();
        CHECK(pending_transfers == 1);
        complete_on_delay = true;
        ops->destroy(owner);
        CHECK(!complete_on_delay && ready_calls == ready + 1);
        CHECK(event_position("refresh_stop") < event_position("callback"));
        CHECK(event_position("callback") < event_position("view_delete"));
        CHECK(event_position("flush_ready") < event_position("io_delete"));
        CHECK(event_position("io_delete") < event_position("display_remove"));
        assert_all_released();
        ops->destroy(owner);
        assert_all_released();
    }
}

static void test_adapter_flush_timeout_and_retry() {
    const mybot_lcd_ops_t *ops = mybot_shared_lvgl_ops();
    void *context = nullptr;
    CHECK(ops->init(&context) == 0);
    submit_flush();
    const int64_t before = time_us;
    const unsigned int removes = display_removes;
    ops->destroy(context);
    CHECK(time_us - before == 1000000);
    CHECK(pending_transfers == 1 && display.open && io.open && panel.open && spi_open);
    CHECK(!display.refresh && !timer.open && view_open && display_removes == removes);
    mybot_lcd_content_t content{};
    content.screen = MYBOT_LCD_SCREEN_READY;
    CHECK(ops->render(context, &content) < 0);
    void *retry = reinterpret_cast<void *>(1);
    CHECK(ops->init(&retry) < 0 && !retry);
    CHECK(display_removes == removes && pending_transfers == 1);
    complete_transfer();
    CHECK(ops->init(&retry) == 0 && retry == context);
    CHECK(display_removes == removes + 1);
    ops->destroy(retry);
    assert_all_released();
}

static void test_adapter_cleanup_failures_and_draw_error() {
    const mybot_lcd_ops_t *ops = mybot_shared_lvgl_ops();
    for (const char *failure :
         {"port_lock", "panel_delete", "io_delete", "spi_free", "display_remove", "port_deinit"}) {
        void *context = nullptr;
        CHECK(ops->init(&context) == 0);
        failed_step = failure;
        ops->destroy(context);
        CHECK(failed_step.empty());
        ops->destroy(context);
        assert_all_released();
    }
    void *context = nullptr;
    CHECK(ops->init(&context) == 0);
    failed_step = "draw";
    const unsigned int ready = ready_calls;
    submit_flush();
    CHECK(pending_transfers == 0 && ready_calls == ready + 1);
    const int64_t before = time_us;
    ops->destroy(context);
    CHECK(time_us == before);
    assert_all_released();
}

int main() {
    test_panel_initialization_failures();
    test_panel_cleanup_retry_and_ordering();
    test_adapter_initialization_failures();
    test_adapter_references_and_callback_ordering();
    test_adapter_flush_timeout_and_retry();
    test_adapter_cleanup_failures_and_draw_error();
    std::puts("ST7789/shared LVGL host fault-injection tests passed");
    return 0;
}
