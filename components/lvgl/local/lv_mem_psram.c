/* SPDX-License-Identifier: Apache-2.0 */
/* Local adapter implementing LVGL's documented custom allocator contract. */
#include "lvgl.h"

#include "esp_heap_caps.h"

#include <stddef.h>
#include <stdint.h>

#define LVGL_MEMORY_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

typedef union allocation_header allocation_header_t;
union allocation_header {
    max_align_t alignment;
    struct {
        allocation_header_t *previous;
        allocation_header_t *next;
    } links;
};

/* The port serializes all LVGL allocation calls. Keep the list so deinit also
 * releases allocations left behind by an interrupted initialization. */
static allocation_header_t *s_allocations;

void lv_mem_init(void) {
}

void lv_mem_deinit(void) {
    while (s_allocations) {
        allocation_header_t *next = s_allocations->links.next;
        heap_caps_free(s_allocations);
        s_allocations = next;
    }
}

lv_mem_pool_t lv_mem_add_pool(void *memory, size_t bytes) {
    (void)memory;
    (void)bytes;
    return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool) {
    (void)pool;
}

void *lv_malloc_core(size_t size) {
    if (size > SIZE_MAX - sizeof(allocation_header_t)) {
        return NULL;
    }
    allocation_header_t *header = heap_caps_malloc(sizeof(*header) + size, LVGL_MEMORY_CAPS);
    if (!header) {
        return NULL;
    }
    header->links.previous = NULL;
    header->links.next = s_allocations;
    if (s_allocations) {
        s_allocations->links.previous = header;
    }
    s_allocations = header;
    return header + 1;
}

void lv_free_core(void *pointer) {
    if (!pointer) {
        return;
    }
    allocation_header_t *header = (allocation_header_t *)pointer - 1;
    if (header->links.previous) {
        header->links.previous->links.next = header->links.next;
    } else {
        s_allocations = header->links.next;
    }
    if (header->links.next) {
        header->links.next->links.previous = header->links.previous;
    }
    heap_caps_free(header);
}

void *lv_realloc_core(void *pointer, size_t size) {
    if (!pointer) {
        return lv_malloc_core(size);
    }
    if (size == 0) {
        lv_free_core(pointer);
        return NULL;
    }
    if (size > SIZE_MAX - sizeof(allocation_header_t)) {
        return NULL;
    }
    allocation_header_t *header = (allocation_header_t *)pointer - 1;
    allocation_header_t *previous = header->links.previous;
    allocation_header_t *next = header->links.next;
    allocation_header_t *replacement =
        heap_caps_realloc(header, sizeof(*header) + size, LVGL_MEMORY_CAPS);
    if (!replacement) {
        return NULL;
    }
    replacement->links.previous = previous;
    replacement->links.next = next;
    if (previous) {
        previous->links.next = replacement;
    } else {
        s_allocations = replacement;
    }
    if (next) {
        next->links.previous = replacement;
    }
    return replacement + 1;
}

void lv_mem_monitor_core(lv_mem_monitor_t *monitor) {
    multi_heap_info_t info;
    heap_caps_get_info(&info, LVGL_MEMORY_CAPS);
    monitor->total_size = info.total_free_bytes + info.total_allocated_bytes;
    monitor->free_cnt = info.free_blocks;
    monitor->free_size = info.total_free_bytes;
    monitor->free_biggest_size = info.largest_free_block;
    monitor->used_cnt = info.allocated_blocks;
    monitor->max_used = monitor->total_size - info.minimum_free_bytes;
    monitor->used_pct =
        monitor->total_size ? (uint8_t)(100 * info.total_allocated_bytes / monitor->total_size) : 0;
    monitor->frag_pct = info.total_free_bytes
                            ? (uint8_t)(100 - 100 * info.largest_free_block / info.total_free_bytes)
                            : 0;
}

lv_result_t lv_mem_test_core(void) {
    return LV_RESULT_OK;
}
