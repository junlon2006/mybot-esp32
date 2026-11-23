/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_APP_TEST_HOST_IDF_H_
#define MYBOT_APP_TEST_HOST_IDF_H_

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

typedef int esp_err_t;
enum {
    ESP_OK,
    ESP_ERR_INVALID_STATE,
    ESP_ERR_NVS_NO_FREE_PAGES,
    ESP_ERR_NVS_NEW_VERSION_FOUND,
    ESP_MAC_WIFI_STA,
    IP_EVENT_STA_GOT_IP,
};
#define ESP_ERROR_CHECK(result) assert((result) == ESP_OK)
#define ESP_LOGI(tag, ...) host_log(tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) host_log(tag, __VA_ARGS__)
#define ESP_LOGE(tag, ...) host_log(tag, __VA_ARGS__)

void host_log(const char *tag, const char *format, ...) __attribute__((format(printf, 2, 3)));
int64_t esp_timer_get_time(void);
const char *mybot_version_string(void);

typedef unsigned int TickType_t;
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
void vTaskDelay(TickType_t ticks);

typedef struct {
    const char *date;
    const char *time;
    const char *version;
} esp_app_desc_t;

static inline const esp_app_desc_t *esp_app_get_description(void) {
    static const esp_app_desc_t description = {
        .date = __DATE__, .time = __TIME__, .version = "host-test"};
    return &description;
}

static inline const char *esp_err_to_name(esp_err_t error) {
    (void)error;
    return "error";
}

static inline const char *esp_get_idf_version(void) {
    return "host";
}

static inline esp_err_t nvs_flash_init(void) {
    return ESP_OK;
}

static inline esp_err_t nvs_flash_erase(void) {
    return ESP_OK;
}

static inline esp_err_t esp_netif_init(void) {
    return ESP_OK;
}

static inline esp_err_t esp_event_loop_create_default(void) {
    return ESP_OK;
}

static inline bool esp_psram_is_initialized(void) {
    return true;
}

static inline esp_err_t esp_read_mac(uint8_t *mac, int type) {
    assert(type == ESP_MAC_WIFI_STA);
    memset(mac, 0, 6);
    return ESP_OK;
}

typedef const char *esp_event_base_t;
typedef void (*esp_event_handler_instance_t)(void *, esp_event_base_t, int32_t, void *);
#define IP_EVENT "IP_EVENT"

esp_err_t esp_event_handler_instance_register(esp_event_base_t base, int32_t id,
                                              esp_event_handler_instance_t handler, void *arg,
                                              esp_event_handler_instance_t *instance);
esp_err_t esp_event_handler_instance_unregister(esp_event_base_t base, int32_t id,
                                                esp_event_handler_instance_t instance);

typedef struct {
    bool start;
    bool wait_for_sync;
    void (*sync_cb)(struct timeval *);
} esp_sntp_config_t;

#define ESP_NETIF_SNTP_DEFAULT_CONFIG(server) ((esp_sntp_config_t){0})
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *config);
esp_err_t esp_netif_sntp_start(void);
void esp_netif_sntp_deinit(void);

typedef int psa_status_t;
typedef unsigned int psa_key_id_t;
typedef unsigned int psa_key_attributes_t;

#define PSA_KEY_ATTRIBUTES_INIT 0
#define PSA_KEY_TYPE_HMAC 0
#define PSA_ALG_SHA_256 0
#define PSA_ALG_HMAC(algorithm) (algorithm)
#define PSA_KEY_USAGE_SIGN_MESSAGE 0
#define PSA_SUCCESS 0

static inline void psa_set_key_type(psa_key_attributes_t *attributes, unsigned int type) {
    (void)attributes;
    (void)type;
}

static inline void psa_set_key_algorithm(psa_key_attributes_t *attributes, unsigned int algorithm) {
    (void)attributes;
    (void)algorithm;
}

static inline void psa_set_key_usage_flags(psa_key_attributes_t *attributes, unsigned int flags) {
    (void)attributes;
    (void)flags;
}

static inline psa_status_t psa_import_key(const psa_key_attributes_t *attributes,
                                          const uint8_t *data, size_t length, psa_key_id_t *key) {
    (void)attributes;
    (void)data;
    assert(length > 0);
    *key = 1;
    return PSA_SUCCESS;
}

static inline void psa_reset_key_attributes(psa_key_attributes_t *attributes) {
    (void)attributes;
}

static inline psa_status_t psa_mac_compute(psa_key_id_t key, unsigned int algorithm,
                                           const uint8_t *input, size_t input_length, uint8_t *mac,
                                           size_t mac_size, size_t *mac_length) {
    (void)algorithm;
    (void)input;
    assert(key == 1 && input_length == 6);
    memset(mac, 0, mac_size);
    *mac_length = mac_size;
    return PSA_SUCCESS;
}

static inline psa_status_t psa_destroy_key(psa_key_id_t key) {
    assert(key == 1);
    return PSA_SUCCESS;
}

#endif /* MYBOT_APP_TEST_HOST_IDF_H_ */
