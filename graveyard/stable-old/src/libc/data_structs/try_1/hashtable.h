#include <klib/string.h>
#include "list.h"
#include "item_ops.h"


typedef struct hashtable_struct hashtable_t;
typedef void void_key_item_consumer(char *key, void *item);


// hashtable guarantees the get/set operations to be O(1) at best case
struct hashtable_struct {
    void *priv_data;

    bool (*empty)(hashtable_t *hashtable);
    int (*size)(hashtable_t *hashtable);
    void (*clear)(hashtable_t *hashtable);

    bool (*contains)(hashtable_t *hashtable, char *key);
    void *(*get)(hashtable_t *hashtable, char *key);
    void (*set)(hashtable_t *hashtable, char *key, void *item);
    void (*delete)(hashtable_t *hashtable, char *key);
    
    // one way of doing this
    void (*for_each)(hashtable_t *hashtable, void_key_item_consumer *consumer);

    // an advanced functionality!
    list_t *(*create_keys_list)(hashtable_t *hashtable);
};


hashtable_t *create_hashtable(struct item_operations *operations, int capacity);
void free_hashtable(hashtable_t *hashtable);
