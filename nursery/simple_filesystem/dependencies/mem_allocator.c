#include "mem_allocator.h"
#include <stdlib.h>


static void *allocate(mem_allocator *m, uint32_t size) {
    return malloc(size);
}

static void release(mem_allocator *m, void *ptr) {
    free(ptr);
}

mem_allocator *new_malloc_based_mem_allocator() {
    mem_allocator *m = malloc(sizeof(mem_allocator));
    m->allocate = allocate;
    m->release = release;
    return m;
}

