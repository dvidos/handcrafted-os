#include <klib/string.h>
#include "list.h"
#include "hashtable.h"
#include "item_ops.h"


struct hashtable_node {
    char *key;
    void *item;
    struct hashtable_node *next;
};

struct hashtable_priv_data {
    struct item_operations *ops;
    int capacity;
    int size;

    // dynamically allocated array
    struct hashtable_node *nodes_array;
};

static struct hashtable_node *get_first_node_slot(struct hashtable_priv_data *data, char *key) {
    int hash = 0;
    while (*key != '\0')
        hash = hash * 31 + *key++; // 31 is prime, creates better hashes
    
    int slot = hash % data->capacity;
    return &data->nodes_array[slot];
}

static char *clone_str(char *str) {
    char *ptr = malloc(strlen(str) + 1);
    strcpy(ptr, str);
    return ptr;
}

static bool hashtable_empty(hashtable_t *hashtable) {
    struct hashtable_priv_data *data = (struct hashtable_priv_data *)hashtable->priv_data;
    return data->size == 0;
}

static int hashtable_size(hashtable_t *hashtable) {
    struct hashtable_priv_data *data = (struct hashtable_priv_data *)hashtable->priv_data;
    return data->size;
}

static void hashtable_clear(hashtable_t *hashtable) {
    // make sure to free items
}

static bool hashtable_contains(hashtable_t *hashtable, char *key) {
    struct hashtable_priv_data *data = (struct hashtable_priv_data *)hashtable->priv_data;
    struct hashtable_node *node = get_first_node_slot(data, key);

    // maybe the slot is completely empty
    if (node->key == NULL)
        return false;

    // if more than one items in the slot, follow the linked list
    while (node != NULL) {
        if (strcmp(key, node->key) == 0)
            return true;
        node = node->next;
    }

    return false;
}

static void *hashtable_get(hashtable_t *hashtable, char *key) {
    struct hashtable_priv_data *data = (struct hashtable_priv_data *)hashtable->priv_data;
    struct hashtable_node *node = get_first_node_slot(data, key);

    // maybe the slot is completely empty
    if (node->key == NULL)
        return NULL;

    // if more than one items in the slot, follow the linked list
    while (node != NULL) {
        if (strcmp(key, node->key) == 0)
            return node->item;
        node = node->next;
    }

    return NULL;
}

static void hashtable_set(hashtable_t *hashtable, char *key, void *item) {
    struct hashtable_priv_data *data = (struct hashtable_priv_data *)hashtable->priv_data;
    struct hashtable_node *node = get_first_node_slot(data, key);

    // maybe the slot is completely empty
    if (node->key == NULL) {
        node->key = clone_str(key);
        node->item = data->ops->clone(data->ops, item);
        node->next = NULL;
        return;
    }

    // we need to set or add a node
    while (node != NULL) {
        if (strcmp(key, node->key) == 0) {
            // free existing item, then clone the new one to update in place
            data->ops->free(data->ops, node->item);
            node->item = data->ops->clone(data->ops, item);
            return;
        }
        if (node->next == NULL) {
            // it does not exist yet.
            struct hashtable_node *new_node = malloc(sizeof(struct hashtable_node));
            new_node->key = clone_str(key);
            new_node->item = data->ops->clone(data->ops, item);
            new_node->next = NULL;
            node->next = new_node;
            return;
        }
        node = node->next;
    }
}

static void hashtable_delete(hashtable_t *hashtable, char *key) {
    // a bit more involved, especially in the linked list.
    struct hashtable_priv_data *data = (struct hashtable_priv_data *)hashtable->priv_data;
    struct hashtable_node *node = get_first_node_slot(data, key);

    // first, easy case, the only entry in the array
    if (strcmp(key, node->key) == 0) {
        // free the content first
        free(node->key);
        node->key = NULL;
        data->ops->free(data->ops, node->item);
        node->item = NULL;

        // if that was not the only entry, bring data 
        // from the next node, then free the next node
        if (node->next != NULL) {
            struct hashtable_node *to_delete = node->next;
            node->key = to_delete->key;
            node->item = to_delete->item;
            node->next = to_delete->next;
            free(to_delete);
        }

        // we're done anyway
        return;
    }

    // so, not found on the base node
    // maintain a trailing pointer, to remove the node, if we find it.
    struct hashtable_node *trailing = node;
    struct hashtable_node *target = node->next;
    while (target != NULL) {
        if (strcmp(target, key) == 0) {
            free(target->key);
            data->ops->free(data->ops, target->item);
            trailing->next = target->next;
            free(target);
            return;
        }
        trailing = target;
        target = target->next;
    }
}

static void hashtable_for_each(hashtable_t *hashtable, void_key_item_consumer *consumer) {
    struct hashtable_priv_data *data = (struct hashtable_priv_data *)hashtable->priv_data;

    for (int slot = 0; slot < data->capacity; slot++) {
        struct hashtable_node *node = &data->nodes_array[slot];
        while (node != NULL && node->key != NULL) {
            consumer(node->key, node->item);
            node = node->next;
        }
    }
}

static list_t *hashtable_create_keys_list(hashtable_t *hashtable) {
    struct hashtable_priv_data *data = (struct hashtable_priv_data *)hashtable->priv_data;
    struct item_operations *str_ops = create_str_item_operations();
    list_t *list = create_list(str_ops);

    for (int slot = 0; slot < data->capacity; slot++) {
        struct hashtable_node *node = &data->nodes_array[slot];
        while (node != NULL) {
            list->append(list, node->key);
            node = node->next;
        }
    }

    return list;
}

hashtable_t *create_hashtable(struct item_operations *operations, int capacity) {
    struct hashtable_priv_data *priv_data = malloc(sizeof(struct hashtable_priv_data));
    priv_data->ops = operations;
    priv_data->capacity = capacity;
    priv_data->size = 0;

    int array_size = sizeof(struct hashtable_node) * capacity;
    priv_data->nodes_array = malloc(array_size);
    memset(priv_data->nodes_array, 0, array_size);

    hashtable_t *hashtable = malloc(sizeof(hashtable_t));
    hashtable->priv_data = priv_data;

    hashtable->empty = hashtable_empty;
    hashtable->size = hashtable_size;
    hashtable->clear = hashtable_clear;
    hashtable->contains = hashtable_contains;
    hashtable->get = hashtable_get;
    hashtable->set = hashtable_set;
    hashtable->delete = hashtable_delete;
    hashtable->for_each = hashtable_for_each;
    hashtable->create_keys_list = hashtable_create_keys_list;

    return hashtable;
}

void free_hashtable(hashtable_t *hashtable) {
    struct hashtable_priv_data *data = (struct hashtable_priv_data *)hashtable->priv_data;

    // make sure all items are freed first
    hashtable_clear(hashtable);

    // we don't free item operations, we did not allocate it.
    free(data->nodes_array);
    free(data);
    free(hashtable);
}
