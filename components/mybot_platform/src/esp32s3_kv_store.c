/* SPDX-License-Identifier: Apache-2.0 */
#include <mybot/platform/mybot_kv_store.h>

#include "esp_log.h"
#include "nvs.h"

#include <stdbool.h>
#include <stdint.h>

#define TAG "mybot_kv"
#define MYBOT_NVS_NAMESPACE "mybot"

typedef struct {
    nvs_handle_t handle;
    bool open;
} kv_context_t;

static kv_context_t s_context;

static int kv_init(void **out_ctx) {
    if (!out_ctx || s_context.open) {
        return -1;
    }
    *out_ctx = NULL;
    if (nvs_open(MYBOT_NVS_NAMESPACE, NVS_READWRITE, &s_context.handle) != ESP_OK) {
        return -1;
    }
    s_context.open = true;
    *out_ctx = &s_context;
    return 0;
}

static int kv_get(void *opaque, const char *key, void *value, size_t capacity, size_t *out_len) {
    kv_context_t *ctx = opaque;
    if (ctx != &s_context || !ctx->open || !key || !value || !out_len) {
        return -1;
    }

    size_t required = 0;
    esp_err_t err = nvs_get_blob(ctx->handle, key, NULL, &required);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return MYBOT_ERR_NOT_FOUND;
    }
    if (err != ESP_OK || required > capacity) {
        return -1;
    }

    size_t actual = capacity;
    err = nvs_get_blob(ctx->handle, key, value, &actual);
    if (err != ESP_OK) {
        return -1;
    }
    *out_len = actual;
    return 0;
}

static int kv_set(void *opaque, const char *key, const void *value, size_t len) {
    kv_context_t *ctx = opaque;
    if (ctx != &s_context || !ctx->open || !key || (!value && len != 0)) {
        return -1;
    }
    if (nvs_set_blob(ctx->handle, key, value, len) != ESP_OK) {
        return -1;
    }
    return nvs_commit(ctx->handle) == ESP_OK ? 0 : -1;
}

static int kv_erase(void *opaque, const char *key) {
    kv_context_t *ctx = opaque;
    if (ctx != &s_context || !ctx->open || !key) {
        return -1;
    }
    esp_err_t err = nvs_erase_key(ctx->handle, key);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        return -1;
    }
    return nvs_commit(ctx->handle) == ESP_OK ? 0 : -1;
}

static void kv_destroy(void *opaque) {
    kv_context_t *ctx = opaque;
    if (ctx != &s_context || !ctx->open) {
        return;
    }
    nvs_close(ctx->handle);
    ctx->open = false;
}

static const mybot_kv_store_ops_t s_ops = {
    .name = "esp-idf-nvs",
    .init = kv_init,
    .get = kv_get,
    .set = kv_set,
    .erase = kv_erase,
    .destroy = kv_destroy,
};

int mybot_esp32s3_kv_store_register(void) {
    int result = mybot_kv_store_register(&s_ops);
    if (result < 0) {
        ESP_LOGE(TAG, "KV registration failed");
    }
    return result;
}
