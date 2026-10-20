/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MCP_TRACE_MOCK_ESP_HEAP_CAPS_H_
#define MCP_TRACE_MOCK_ESP_HEAP_CAPS_H_

#include <stddef.h>
#include <stdint.h>

#define MALLOC_CAP_SPIRAM (1U << 10)
#define MALLOC_CAP_8BIT (1U << 2)

void *heap_caps_malloc(size_t size, uint32_t caps);
void heap_caps_free(void *pointer);

#endif /* MCP_TRACE_MOCK_ESP_HEAP_CAPS_H_ */
