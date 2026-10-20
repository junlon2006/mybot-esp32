/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MCP_TRACE_MOCK_ESP_LOG_H_
#define MCP_TRACE_MOCK_ESP_LOG_H_

void mock_esp_log(char severity, const char *tag, const char *format, ...)
    __attribute__((format(printf, 3, 4)));

#define ESP_LOGE(tag, ...) mock_esp_log('E', tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) mock_esp_log('W', tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) mock_esp_log('I', tag, __VA_ARGS__)

#endif /* MCP_TRACE_MOCK_ESP_LOG_H_ */
