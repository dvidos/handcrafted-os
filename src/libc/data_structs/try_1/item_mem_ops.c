#include <string.h>
#include "item_ops.h"

struct mem_item_ops_data {
    int fixed_size;
};

static int mem_equals(struct item_operations *ops, void *item, void *target) {
    struct mem_item_ops_data *data = (struct mem_item_ops_data *)ops->priv_data;
    return memcmp(item, target, data->fixed_size) == 0;
}

static void *mem_clone(struct item_operations *ops, void *item) {
    struct mem_item_ops_data *data = (struct mem_item_ops_data *)ops->priv_data;
    unsigned char *clone = malloc(data->fixed_size);
    memcpy(clone, item, data->fixed_size);
    return clone;
};

static void mem_free(struct item_operations *ops, void *item) {
    free(item);
};

static bool mem_matches(struct item_operations *ops, void *item, void *pattern) {
    // lots of room for custom logic here
    struct mem_item_ops_data *data = (struct mem_item_ops_data *)ops->priv_data;
    return memcmp(item, pattern, data->fixed_size) == 0;
}

static int mem_compare(struct item_operations *ops, void *item_a, void *item_b) {
    struct mem_item_ops_data *data = (struct mem_item_ops_data *)ops->priv_data;
    return memcmp(item_a, item_b, data->fixed_size);
}

static int mem_hash(struct item_operations *ops, void *item) {
    struct mem_item_ops_data *data = (struct mem_item_ops_data *)ops->priv_data;
    int a = 0;
    unsigned char *p = (unsigned char *)item;
    for (int i = 0; i < data->fixed_size; i++) {
        a = (a * 31) + (int)*p++; // 31 is a prime
    }
    return a;
}

static void *mem_to_string(struct item_operations *ops, void *item) {
    // lots of room for improvement here as well
    struct mem_item_ops_data *data = (struct mem_item_ops_data *)ops->priv_data;
    unsigned char *buffer = malloc(data->fixed_size * 3 + 1);
    for (int i = 0; i < data->fixed_size; i++) {
        sprintfn(buffer + i * 3, "%02x ", ((unsigned char *)item)[i]);
    }
    buffer[data->fixed_size * 3] = '\0';
    return buffer;
}

struct item_operations *create_mem_item_operations(int fixed_size) {
    struct mem_item_ops_data *priv = malloc(sizeof(struct mem_item_ops_data));
    priv->fixed_size = fixed_size;

    struct item_operations *ops = malloc(sizeof(struct item_operations));
    ops->equals = mem_equals;
    ops->clone = mem_clone;
    ops->free = mem_free;
    ops->compare = mem_compare;
    ops->matches = mem_matches;
    ops->hash = mem_hash;
    ops->to_string = mem_to_string;
    ops->priv_data = NULL;
    return ops;
}

void free_mem_item_operations(struct item_operations *ops) {
    free(ops->priv_data);
    free(ops);
}
