#include <ctypes.h>
#include <string.h>
#include "data_structs.h"

#define IS_BIT(value, bitno)       ((value) & (1 << (bitno)))
#define SET_BIT(value, bitno)      ((value) | (1 << (bitno)))
#define CLEAR_BIT(value, bitno)    ((value) & ~(1 << (bitno)))


typedef struct allocation_bucket {
    uint32_t *bitmaps;
    bool all_allocated;
    void *data;
    struct allocation_bucket *next;
} allocation_bucket;

typedef struct allocator_data {
    uint32_t item_size;
    uint32_t bucket_num_items; // multiple of 32, one u32 bitmap per 32 items.
    struct allocation_bucket *first_bucket;
} allocator_data;


static allocation_bucket *create_bucket(allocator_data *data) {
    int bitmaps_bytes = data->bucket_num_items / 8;

    // we allocate a single chunk, for the structure, the bitmaps, and the data area
    int size_to_allocate = 
        sizeof(allocation_bucket) +            // header
        bitmaps_bytes +                        // bitmaps
        (data->bucket_num_items * data->item_size);  // data

    allocation_bucket *bucket = data_structs_malloc(size_to_allocate);
    memset(bucket, 0, size_to_allocate);

    bucket->bitmaps = (uint32_t *)(((char *)bucket) + sizeof(allocation_bucket));
    bucket->data = ((char *)bucket->bitmaps) + bitmaps_bytes;

    return bucket;
}

static void destroy_bucket(allocation_bucket *bucket) {
    data_structs_free(bucket);
}

static void *allocate_item_in_bucket(allocation_bucket *bucket, allocator_data *data) {
    int bitmaps_count = data->bucket_num_items / 32;

    int bitmap_no = 0;
    while (bitmap_no < bitmaps_count && bucket->bitmaps[bitmap_no] == 0xFFFF)
        bitmap_no++;

    // maybe all bits are allocated
    if (bitmap_no >= bitmaps_count){
        bucket->all_allocated = true;
        return NULL;
    }

    int bit_no = 0;
    while (bit_no < 32 && IS_BIT(bucket->bitmaps[bitmap_no], bit_no))
        bit_no++;
    
    if (bit_no >= 32) {
        if (data_structs_warn)
            data_structs_warn("A bitmap with open slots, could not identify bit number");
        return NULL; // this should not happen!
    }

    bucket->bitmaps[bitmap_no] = SET_BIT(bucket->bitmaps[bitmap_no], bit_no);
    void *address = (char *)bucket->data + (((bitmap_no * 32) + bit_no) * data->item_size);

    return address;
}

static bool free_item_in_bucket(allocation_bucket *bucket, allocator_data *data, void *item) {
    // maybe this pointer does not belong to us
    if (item < bucket->data || item >= bucket->data + (data->bucket_num_items * data->item_size))
        return false;
    
    uint32_t offset = (char *)item - (char *)bucket->data;
    int bitmap_no = offset / 32;
    int bit_no = offset % 32;
    bucket->bitmaps[bitmap_no] = CLEAR_BIT(bucket->bitmaps[bitmap_no], bit_no);
    bucket->all_allocated = false;

    return true;
}

static void *allocate_item(item_allocator *a) {
    allocator_data *data = (allocator_data *)a->priv_data;
    void *item;

    // we'll try each bucket to allocate, if not, we'll add a new bucket.
    allocation_bucket *bucket = data->first_bucket;
    while (bucket != NULL) {
        if (bucket->all_allocated) {
            bucket = bucket->next;
            continue;
        }

        item = allocate_item_in_bucket(bucket, data);
        if (item != NULL)
            return item;

        bucket = bucket->next;
    }

    // if we are here, we could not allocate from existing buckets, 
    // insert new bucket in chain
    bucket = create_bucket(data);
    bucket->next = data->first_bucket;
    data->first_bucket = bucket;

    item = allocate_item_in_bucket(bucket, data);
    if (item == NULL) {
        if (data_structs_warn)
            data_structs_warn("Could not allocate a slot in a newly created bucket!");
        return NULL;
    }
    
    return item;
}

static void free_item(item_allocator *a, void *item) {
    allocator_data *data = (allocator_data *)a->priv_data;

    // we'll try each bucket for deallocation
    allocation_bucket *bucket = data->first_bucket;
    while (bucket != NULL) {
        bool found = free_item_in_bucket(bucket, data, item);
        if (found)
            return;

        bucket = bucket->next;
    }

    // if we are here, the pointer does not belong to us.
    if (data_structs_warn) 
        data_structs_warn("Item to free does belong to any bucket (0x%x)", item);
}

static void destroy_item_allocator(item_allocator *a) {
    allocator_data *data = (allocator_data *)a->priv_data;

    allocation_bucket *bucket = data->first_bucket;
    while (bucket != NULL) {
        allocation_bucket *next = bucket->next;
        destroy_bucket(bucket);
        bucket = next;
    }

    data_structs_free(data);
    data_structs_free(a);
}

item_allocator *create_item_allocator(int item_size) {
    item_allocator *a = data_structs_malloc(sizeof(item_allocator));
    allocator_data *data = data_structs_malloc(sizeof(allocator_data));

    data->item_size = item_size;

    // number of items per bucket must be a multiple of 32.
    // it can be optimized for smaller or larger data sets
    data->bucket_num_items = 4 * 32;
    data->first_bucket = create_bucket(data);

    a->priv_data = data;
    a->allocate = allocate_item;
    a->free = free_item;
    a->destroy = destroy_item_allocator;

    return a;
}
