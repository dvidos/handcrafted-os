#include "../include/ctypes.h"
#include "../klib/string.h"
#include "../memory/kheap.h"

typedef struct {
    int item_size;  // item size in bytes
    int capacity;   // capacity in items
    int head;       // write head
    int tail;       // read head
    int count;      // items in buffer
    char data[];    // malloc'ed buffer
} ring_buffer_t;


ring_buffer_t *create_ring_buffer(int item_size, int num_items) {
    int mem_size = sizeof(ring_buffer_t) + item_size * num_items;
    ring_buffer_t *buffer = kmalloc(mem_size);
    if (buffer == NULL)
        return NULL;
    
    memset(buffer, 0, mem_size);
    return buffer;
}

bool ring_buffer_is_full(ring_buffer_t *buffer) {
    return buffer->count == buffer->capacity;
}

bool ring_buffer_is_empty(ring_buffer_t *buffer) {
    return buffer->count == 0;
}

bool ring_buffer_enqueue(ring_buffer_t *buffer, void *item) {
    if (buffer->count == buffer->capacity)
        return false;
    
    memcpy(buffer->data + (buffer->head * buffer->item_size), item, buffer->item_size);
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->count++;
    return true;
}

bool ring_buffer_dequeue(ring_buffer_t *buffer, void *item) {
    if (buffer->count == 0)
        return false;
    
    memcpy(item, buffer->data + buffer->tail, buffer->item_size);
    buffer->tail = (buffer->tail + 1) % buffer->capacity;
    buffer->count--;
    return true;
}

void ring_buffer_destroy(ring_buffer_t *buffer) {
    kfree(buffer);
}
