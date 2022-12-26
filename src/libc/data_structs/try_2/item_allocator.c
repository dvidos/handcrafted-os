#include <ctypes.h>
#include <bits.h>
#include "data_structs.h"



// --------------------------------------------------------

typedef struct item_allocator_data {
    int item_size;
    int max_items;
    int bitmaps_bytes;
    unsigned char *bitmaps;
    void *items_buffer;
    struct item_allocator_data *next;
} item_allocator_data;


void item_allocator_destroy(item_allocator *a);
void *item_allocator_allocate(item_allocator *a);
void item_allocator_free(item_allocator *a, void *address);


item_allocator *create_item_allocator(int item_size, int max_items_needed) {
    item_allocator *a = malloc(sizeof(item_allocator));
    item_allocator_data *d = malloc(sizeof(item_allocator_data));

    // round items needed up to nearest 8
    d->max_items = (max_items_needed + 7) & ~0x7;
    
    d->item_size = item_size;
    d->bitmaps_bytes = max_items_needed / 8;
    d->bitmaps = malloc(d->bitmaps_bytes + (d->item_size * d->max_items));
    d->items_buffer = d->bitmaps + d->bitmaps_bytes;
    d->next = NULL;

    a->allocate = item_allocator_allocate;
    a->free = item_allocator_free;
    a->data = d;
    a->destroy = item_allocator_destroy;

    return a;
}

static void item_allocator_destroy(item_allocator *a) {
    free(((item_allocator_data *)a->data)->bitmaps);
    free(a->data);
    free(a);
}

static void *item_allocator_allocate(item_allocator *a) {
    item_allocator_data *d = (item_allocator_data *)a->data;

    for (int byte_no = 0; byte_no < d->bitmaps_bytes; byte_no++) {
        if (d->bitmaps[byte_no] == 0xFF)
            continue;
        
        // this octet has at least one bit free
        for (int bit_no = 7; bit_no >= 0; bit_no--) {
            if (IS_BIT(d->bitmaps[byte_no], bit_no))
                continue;

            // we found a free bit
            d->bitmaps[byte_no] = SET_BIT(d->bitmaps[byte_no], bit_no);
            return d->items_buffer + (((byte_no * 8) + bit_no) * d->item_size);
        }
    }

    // we should log something,
    // or we could malloc new areas and new bitmaps...
    // and maintain a linked list of areas with bitmaps...
    return NULL;
}

static void item_allocator_free(item_allocator *a, void *address) {
    item_allocator_data *d = (item_allocator_data *)a->data;

    // see if pointer is actually ours
    if (address < d->items_buffer || address >= d->items_buffer + (d->item_size * d->max_items)) {
        // we should log something
        return;
    }

    // find the byte_no and bit_no based on offset.
    int item_no = (address - d->items_buffer) / d->item_size;
    int byte_no = item_no / 8;
    int bit_no = item_no % 8;

    if (!IS_BIT(d->bitmaps[byte_no], bit_no)) {
        // at last log some error?
        return;
    }
    d->bitmaps[byte_no] = CLEAR_BIT(d->bitmaps[byte_no], bit_no);
}
