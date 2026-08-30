/* SPDX-License-Identifier: MIT */
#include "dns_server.h"
#include <esp_log.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

#define TAG "DnsServer"

DnsServer::DnsServer() {
    stopped_ = xSemaphoreCreateBinaryStatic(&stopped_storage_);
}

DnsServer::~DnsServer() {
    Stop();
    if (stopped_) {
        vSemaphoreDelete(stopped_);
        stopped_ = nullptr;
    }
}

void DnsServer::Start(esp_ip4_addr_t gateway) {
    // If already running, stop first
    if (running_) {
        Stop();
    }

    ESP_LOGI(TAG, "Starting DNS server");
    gateway_ = gateway;

    fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd_ < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        return;
    }

    const struct timeval receive_timeout = {
        .tv_sec = 0,
        .tv_usec = 200000,
    };
    if (setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout)) < 0) {
        ESP_LOGE(TAG, "Failed to configure DNS socket timeout");
        close(fd_);
        fd_ = -1;
        return;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port_);

    if (bind(fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "failed to bind port %d", port_);
        close(fd_);
        fd_ = -1;
        return;
    }

    (void)xSemaphoreTake(stopped_, 0);
    task_fd_ = fd_;
    running_ = true;
    if (xTaskCreate(
            [](void *arg) {
                DnsServer *dns_server = static_cast<DnsServer *>(arg);
                dns_server->Run(dns_server->task_fd_);
                xSemaphoreGive(dns_server->stopped_);
                vTaskDelete(NULL);
            },
            "DnsServerTask", 4096, this, 5, &task_handle_) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create DNS server task");
        running_ = false;
        close(fd_);
        fd_ = -1;
        task_fd_ = -1;
    }
}

void DnsServer::Stop() {
    if (!running_) {
        return;
    }

    ESP_LOGI(TAG, "Stopping DNS server");
    running_ = false;

    // The socket receive timeout bounds how long this join can wait without DNS traffic.
    if (task_handle_ != nullptr) {
        xSemaphoreTake(stopped_, portMAX_DELAY);
        task_handle_ = nullptr;
    }
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
    task_fd_ = -1;
}

void DnsServer::Run(int socket_fd) {
    constexpr int kDnsHeaderSize = 12;
    constexpr int kDnsAnswerSize = 16;
    char buffer[512];
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int len = recvfrom(socket_fd, buffer, sizeof(buffer) - kDnsAnswerSize, 0,
                           (struct sockaddr *)&client_addr, &client_addr_len);
        if (len < 0) {
            if (!running_) {
                // Stop was requested while recvfrom was blocked.
                break;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            ESP_LOGE(TAG, "recvfrom failed, errno=%d", errno);
            continue;
        }
        if (len < kDnsHeaderSize) {
            ESP_LOGW(TAG, "Ignoring malformed DNS request");
            continue;
        }

        if (!running_) {
            break;
        }

        // Simple DNS response: point all queries to 192.168.4.1
        buffer[2] |= 0x80; // Set response flag
        buffer[3] |= 0x80; // Set Recursion Available
        buffer[7] = 1;     // Set answer count to 1

        // Add answer section
        memcpy(&buffer[len], "\xc0\x0c", 2); // Name pointer
        len += 2;
        memcpy(&buffer[len], "\x00\x01\x00\x01\x00\x00\x00\x1c\x00\x04",
               10); // Type, class, TTL, data length
        len += 10;
        memcpy(&buffer[len], &gateway_.addr, 4); // 192.168.4.1
        len += 4;
        ESP_LOGI(TAG, "Sending DNS response to %s", inet_ntoa(gateway_.addr));

        sendto(socket_fd, buffer, len, 0, (struct sockaddr *)&client_addr, client_addr_len);
    }

    ESP_LOGI(TAG, "DNS server task exiting");
}
