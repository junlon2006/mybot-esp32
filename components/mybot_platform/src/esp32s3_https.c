/* SPDX-License-Identifier: Apache-2.0 */
#include <mybot/platform/mybot_https.h>

#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_tls.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define TAG "mybot_https"

typedef struct {
    esp_tls_t *tls;
    int socket_fd;
} https_connection_t;

static int set_socket_timeout(https_connection_t *connection, int option, int timeout_ms) {
    struct timeval timeout = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000,
    };
    return setsockopt(connection->socket_fd, SOL_SOCKET, option, &timeout, sizeof(timeout));
}

static int https_connect(void **out_connection, const char *host, uint16_t port, int timeout_ms) {
    if (!out_connection || !host || !host[0] || port == 0 || timeout_ms <= 0) {
        return -1;
    }
    *out_connection = NULL;

    https_connection_t *connection = calloc(1, sizeof(*connection));
    if (!connection) {
        return -1;
    }
    connection->socket_fd = -1;
    connection->tls = esp_tls_init();
    if (!connection->tls) {
        free(connection);
        return -1;
    }

    esp_tls_cfg_t config = {
        .timeout_ms = timeout_ms,
        .common_name = host,
        .skip_common_name = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    int result = esp_tls_conn_new_sync(host, (int)strlen(host), port, &config, connection->tls);
    if (result != 1 || esp_tls_get_conn_sockfd(connection->tls, &connection->socket_fd) != ESP_OK) {
        ESP_LOGE(TAG, "TLS connection to %s:%u failed", host, (unsigned int)port);
        esp_tls_conn_destroy(connection->tls);
        free(connection);
        return -1;
    }

    *out_connection = connection;
    return 0;
}

static int https_send(void *opaque, const void *data, size_t len, int timeout_ms) {
    https_connection_t *connection = opaque;
    if (!connection || !connection->tls || !data || len == 0 || timeout_ms <= 0 ||
        set_socket_timeout(connection, SO_SNDTIMEO, timeout_ms) < 0) {
        return -1;
    }

    ssize_t result = esp_tls_conn_write(connection->tls, data, len);
    if (result == ESP_TLS_ERR_SSL_WANT_READ || result == ESP_TLS_ERR_SSL_WANT_WRITE) {
        return -1;
    }
    return result > 0 && result <= INT_MAX ? (int)result : -1;
}

static int https_recv(void *opaque, void *data, size_t capacity, int timeout_ms) {
    https_connection_t *connection = opaque;
    if (!connection || !connection->tls || !data || capacity == 0 || timeout_ms <= 0 ||
        set_socket_timeout(connection, SO_RCVTIMEO, timeout_ms) < 0) {
        return -1;
    }

    ssize_t result = esp_tls_conn_read(connection->tls, data, capacity);
    if (result == ESP_TLS_ERR_SSL_WANT_READ || result == ESP_TLS_ERR_SSL_WANT_WRITE) {
        return -1;
    }
    if (result == 0) {
        return 0;
    }
    return result > 0 && result <= INT_MAX ? (int)result : -1;
}

static void https_close(void *opaque) {
    https_connection_t *connection = opaque;
    if (!connection) {
        return;
    }
    if (connection->tls) {
        esp_tls_conn_destroy(connection->tls);
    }
    free(connection);
}

static const mybot_https_ops_t s_ops = {
    .connect = https_connect,
    .send = https_send,
    .recv = https_recv,
    .close = https_close,
};

const mybot_https_ops_t *mybot_esp32s3_https_ops(void) {
    return &s_ops;
}
