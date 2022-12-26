#include <klib/string.h>




struct item_operations {
    void *priv_data;

    bool (*equals)(struct item_operations *ops, void *item, void *target);
    void *(*clone)(struct item_operations *ops, void *item);
    void (*free)(struct item_operations *ops, void *item);
    bool (*matches)(struct item_operations *ops, void *item, void *pattern);
    int (*compare)(struct item_operations *ops, void *item_a, void *item_b);
    int (*hash)(struct item_operations *ops, void *item);
    unsigned char *(*to_string)(struct item_operations *ops, void *item);
};

// creates family of operations on zero terminated strings
struct item_operations *create_str_item_operations();
void free_str_item_operations(struct item_operations *ops);

// creates family of operations on fixed size memory blocks
struct item_operations *create_mem_item_operations(int fixed_size);
void free_mem_item_operations(struct item_operations *ops);

