#pragma once

#include <stdint.h>

typedef struct mem_allocator mem_allocator;


typedef struct mem_allocator {
    void *(*allocate)(mem_allocator *allocator_data, uint32_t size);
    void (*release)(mem_allocator *allocator_data, void *ptr);

    void *allocator_data;
} mem_allocator;
