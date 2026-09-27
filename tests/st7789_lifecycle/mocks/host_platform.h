/* SPDX-License-Identifier: MIT */
#ifndef ST7789_HOST_PLATFORM_H_
#define ST7789_HOST_PLATFORM_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_TIMEOUT 0x107
const char *esp_err_to_name(esp_err_t error);
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)

typedef int gpio_num_t;
#define GPIO_MODE_OUTPUT 1
typedef struct {
    uint64_t pin_bit_mask;
    int mode;
    int pull_up_en;
    int pull_down_en;
    int intr_type;
} gpio_config_t;
esp_err_t gpio_config(const gpio_config_t *config);
esp_err_t gpio_set_level(gpio_num_t gpio, uint32_t level);
esp_err_t rtc_gpio_hold_dis(gpio_num_t gpio);
esp_err_t rtc_gpio_deinit(gpio_num_t gpio);
#define GPIO_NUM_NC -1
#define GPIO_NUM_0 0
#define GPIO_NUM_1 1
#define GPIO_NUM_2 2
#define GPIO_NUM_4 4
#define GPIO_NUM_5 5
#define GPIO_NUM_6 6
#define GPIO_NUM_7 7
#define GPIO_NUM_8 8
#define GPIO_NUM_9 9
#define GPIO_NUM_10 10
#define GPIO_NUM_12 12
#define GPIO_NUM_13 13
#define GPIO_NUM_14 14
#define GPIO_NUM_15 15
#define GPIO_NUM_16 16
#define GPIO_NUM_18 18
#define GPIO_NUM_21 21
#define GPIO_NUM_38 38
#define GPIO_NUM_39 39
#define GPIO_NUM_40 40
#define SPI2_HOST 2
#define SPI3_HOST 3
#define SPI_DMA_CH_AUTO 0
#define SPICOMMON_BUSFLAG_QUAD 1
#define ESP_INTR_CPU_AFFINITY_AUTO -1
#define LCD_RGB_ELEMENT_ORDER_RGB 0
#define LCD_RGB_ELEMENT_ORDER_BGR 1
#define LEDC_LOW_SPEED_MODE 0
#define LEDC_TIMER_0 0
#define LEDC_CHANNEL_0 0
#define LEDC_TIMER_10_BIT 10
#define LEDC_TIMER_13_BIT 13
#define LEDC_AUTO_CLK 0
#define LEDC_INTR_DISABLE 0
typedef struct {
    int speed_mode;
    int duty_resolution;
    int timer_num;
    unsigned int freq_hz;
    int clk_cfg;
} ledc_timer_config_t;
typedef struct {
    int gpio_num;
    int speed_mode;
    int channel;
    int intr_type;
    int timer_sel;
    uint32_t duty;
    int hpoint;
} ledc_channel_config_t;
esp_err_t ledc_timer_config(const ledc_timer_config_t *config);
esp_err_t ledc_channel_config(const ledc_channel_config_t *config);
esp_err_t ledc_set_duty(int speed_mode, int channel, uint32_t duty);
esp_err_t ledc_update_duty(int speed_mode, int channel);
typedef int esp_lcd_spi_bus_handle_t;
typedef struct {
    union {
        int mosi_io_num;
        int data0_io_num;
    };
    union {
        int miso_io_num;
        int data1_io_num;
    };
    int sclk_io_num;
    union {
        int quadwp_io_num;
        int data2_io_num;
    };
    union {
        int quadhd_io_num;
        int data3_io_num;
    };
    int data4_io_num;
    int data5_io_num;
    int data6_io_num;
    int data7_io_num;
    int isr_cpu_id;
    int max_transfer_sz;
    unsigned int flags;
} spi_bus_config_t;
esp_err_t spi_bus_initialize(int host, const spi_bus_config_t *config, int dma);
esp_err_t spi_bus_free(int host);

typedef struct host_panel_io *esp_lcd_panel_io_handle_t;
typedef struct host_panel *esp_lcd_panel_handle_t;
typedef struct {
    int unused;
} esp_lcd_panel_io_event_data_t;
typedef bool (*esp_lcd_panel_io_color_trans_done_cb_t)(esp_lcd_panel_io_handle_t,
                                                       esp_lcd_panel_io_event_data_t *, void *);
typedef struct {
    esp_lcd_panel_io_color_trans_done_cb_t on_color_trans_done;
} esp_lcd_panel_io_callbacks_t;
typedef struct {
    int cs_gpio_num;
    int dc_gpio_num;
    int spi_mode;
    unsigned int pclk_hz;
    size_t trans_queue_depth;
    int lcd_cmd_bits;
    int lcd_param_bits;
    struct {
        bool quad_mode;
    } flags;
    esp_lcd_panel_io_color_trans_done_cb_t on_color_trans_done;
    void *user_ctx;
} esp_lcd_panel_io_spi_config_t;
typedef struct {
    int reset_gpio_num;
    int rgb_ele_order;
    int bits_per_pixel;
    void *vendor_config;
} esp_lcd_panel_dev_config_t;
esp_err_t esp_lcd_new_panel_io_spi(esp_lcd_spi_bus_handle_t host,
                                   const esp_lcd_panel_io_spi_config_t *config,
                                   esp_lcd_panel_io_handle_t *out);
esp_err_t esp_lcd_new_panel_st7789(esp_lcd_panel_io_handle_t io,
                                   const esp_lcd_panel_dev_config_t *config,
                                   esp_lcd_panel_handle_t *out);
esp_err_t esp_lcd_panel_io_del(esp_lcd_panel_io_handle_t io);
esp_err_t esp_lcd_panel_del(esp_lcd_panel_handle_t panel);
esp_err_t esp_lcd_panel_reset(esp_lcd_panel_handle_t panel);
esp_err_t esp_lcd_panel_init(esp_lcd_panel_handle_t panel);
esp_err_t esp_lcd_panel_set_gap(esp_lcd_panel_handle_t panel, int x, int y);
esp_err_t esp_lcd_panel_invert_color(esp_lcd_panel_handle_t panel, bool invert);
esp_err_t esp_lcd_panel_swap_xy(esp_lcd_panel_handle_t panel, bool swap);
esp_err_t esp_lcd_panel_mirror(esp_lcd_panel_handle_t panel, bool x, bool y);
esp_err_t esp_lcd_panel_disp_on_off(esp_lcd_panel_handle_t panel, bool on);
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t panel, int x1, int y1, int x2, int y2,
                                    const void *pixels);
esp_err_t esp_lcd_panel_io_register_event_callbacks(esp_lcd_panel_io_handle_t io,
                                                    const esp_lcd_panel_io_callbacks_t *callbacks,
                                                    void *user);

typedef pthread_mutex_t portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER
#define portENTER_CRITICAL(lock) ((void)pthread_mutex_lock(lock))
#define portEXIT_CRITICAL(lock) ((void)pthread_mutex_unlock(lock))
#define pdMS_TO_TICKS(ms) (ms)
#define pdTRUE 1
typedef struct {
    bool taken;
} StaticSemaphore_t;
typedef StaticSemaphore_t *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage);
int xSemaphoreTake(SemaphoreHandle_t semaphore, unsigned int ticks);
int xSemaphoreGive(SemaphoreHandle_t semaphore);
void vTaskDelay(unsigned int ticks);
int64_t esp_timer_get_time(void);

typedef struct {
    int x1;
    int y1;
    int x2;
    int y2;
} lv_area_t;
typedef struct host_display lv_display_t;
typedef struct host_timer lv_timer_t;
typedef struct {
    lv_area_t *area;
} lv_event_t;
typedef void (*lv_display_flush_cb_t)(lv_display_t *, const lv_area_t *, uint8_t *);
typedef void (*lv_timer_cb_t)(lv_timer_t *);
#define LV_EVENT_INVALIDATE_AREA 1
#define LV_COLOR_FORMAT_RGB565 1
#define LVGL_PORT_EVENT_USER 1
lv_area_t *lv_event_get_invalidated_area(lv_event_t *event);
int lv_area_get_size(const lv_area_t *area);
void lv_draw_sw_rgb565_swap(void *pixels, int size);
uint32_t lv_display_get_event_count(lv_display_t *display);
void lv_display_add_event_cb(lv_display_t *display, void (*callback)(lv_event_t *), int code,
                             void *user);
void lv_display_set_flush_cb(lv_display_t *display, lv_display_flush_cb_t callback);
void lv_display_delete_refr_timer(lv_display_t *display);
lv_timer_t *lv_timer_create(lv_timer_cb_t callback, unsigned int period, void *user);
void lv_timer_delete(lv_timer_t *timer);

typedef struct {
    int task_priority;
    int task_affinity;
    int task_stack;
    int task_max_sleep_ms;
    int timer_period_ms;
} lvgl_port_cfg_t;
#define ESP_LVGL_PORT_INIT_CONFIG()                                                                \
    { 0, 0, 0, 0, 0 }
typedef struct {
    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_handle_t panel_handle;
    size_t buffer_size;
    int hres;
    int vres;
    int color_format;
    struct {
        bool buff_dma;
        bool swap_bytes;
    } flags;
} lvgl_port_display_cfg_t;
esp_err_t lvgl_port_init(const lvgl_port_cfg_t *config);
esp_err_t lvgl_port_deinit(void);
bool lvgl_port_lock(unsigned int timeout);
void lvgl_port_unlock(void);
lv_display_t *lvgl_port_add_disp(const lvgl_port_display_cfg_t *config);
esp_err_t lvgl_port_remove_disp(lv_display_t *display);
void lvgl_port_flush_ready(lv_display_t *display);
esp_err_t lvgl_port_task_wake(int event, void *user);
int mybot_board_set_display_backlight(unsigned int percent);

#ifdef __cplusplus
}
#endif
#endif /* ST7789_HOST_PLATFORM_H_ */
