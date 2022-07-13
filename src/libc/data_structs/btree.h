#include <stdbool.h>
#include <stdint.h>
#include <klib/string.h>
#include "list.h"
#include "item_ops.h"


typedef struct btree_struct btree_t;
typedef void void_key_item_consumer(char *key, void *item);


// btree guarantees the get/set operations to be O(lg N) at best case
struct btree_struct {
    void *priv_data;

    bool (*empty)(btree_t *btree);
    int (*size)(btree_t *btree);
    void (*clear)(btree_t *btree);

    bool (*contains)(btree_t *btree, char *key);
    void *(*get)(btree_t *btree, char *key);
    void (*set)(btree_t *btree, char *key, void *item);
    void (*remove)(btree_t *btree, char *key);
    
    void *(*get_min)(btree_t *btree);
    void *(*get_max)(btree_t *btree);
    
    void (*pre_order_traverse)(btree_t *btree, void_key_item_consumer *consumer);
    void (*in_order_traverse)(btree_t *btree, void_key_item_consumer *consumer);
    void (*post_order_traverse)(btree_t *btree, void_key_item_consumer *consumer);
};


btree_t *create_btree(struct item_operations *operations);
void free_btree(btree_t *btree);
