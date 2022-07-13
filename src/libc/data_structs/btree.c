#include <stdbool.h>
#include <stdint.h>
#include <klib/string.h>
#include "list.h"
#include "btree.h"
#include "item_ops.h"


struct btree_node {
    char *key;
    void *item;
    struct btree_node *left;
    struct btree_node *right;
};

struct btree_priv_data {
    struct item_operations *ops;
    int size;

    struct btree_node *root;
};

static char *clone_str(char *str) {
    char *ptr = malloc(strlen(str) + 1);
    strcpy(ptr, str);
    return ptr;
}

static bool btree_empty(btree_t *btree) {
    struct btree_priv_data *data = (struct btree_priv_data *)btree->priv_data;
    return data->size == 0;
}

static int btree_size(btree_t *btree) {
    struct btree_priv_data *data = (struct btree_priv_data *)btree->priv_data;
    return data->size;
}

static void btree_clear(btree_t *btree) {
    // make sure to free items
}

static bool btree_contains(btree_t *btree, char *key) {
    struct btree_priv_data *data = (struct btree_priv_data *)btree->priv_data;
    struct btree_node *node = data->root;

    while (node != NULL) {
        int cmp = strcmp(key, node->key);
        if (cmp == 0)
            return true;
        else if (cmp < 0)
            node = node->left;
        else if (cmp > 0)
            node = node->right;
    }

    return false;
}

static void *btree_get(btree_t *btree, char *key) {
    struct btree_priv_data *data = (struct btree_priv_data *)btree->priv_data;
    struct btree_node *node = data->root;

    while (node != NULL) {
        int cmp = strcmp(key, node->key);
        if (cmp == 0)
            return node->item;
        else if (cmp < 0)
            node = node->left;
        else if (cmp > 0)
            node = node->right;
    }

    return NULL;
}

static void btree_set(btree_t *btree, char *key, void *item) {
    // find the place to insert this
}

static void btree_delete(btree_t *btree, char *key) {
    // ideally, we should keep a red-black tree, but this will suffice for now
}

static void *btree_get_min(btree_t *btree) {
    struct btree_priv_data *data = (struct btree_priv_data *)btree->priv_data;
    struct btree_node *node = data->root;

    if (node == NULL)
        return NULL;
    while (node->left != NULL)
        node = node->left;
    return node->item;
}

static void *btree_get_max(btree_t *btree) {
    struct btree_priv_data *data = (struct btree_priv_data *)btree->priv_data;
    struct btree_node *node = data->root;

    if (node == NULL)
        return NULL;
    while (node->right != NULL)
        node = node->right;
    return node->item;
}

static void _pre_order_traverse_from_node(struct btree_node *node, void_key_item_consumer *consumer) {
    if (node == NULL)
        return;
    consumer(node->key, node->item);
    pre_order_traverse(node->left, consumer);
    pre_order_traverse(node->right, consumer);
}

static void _in_order_traverse_from_node(struct btree_node *node, void_key_item_consumer *consumer) {
    if (node == NULL)
        return;
    _in_order_traverse_from_node(node->left, consumer);
    consumer(node->key, node->item);
    _in_order_traverse_from_node(node->right, consumer);
}

static void _post_order_traverse_from_node(struct btree_node *node, void_key_item_consumer *consumer) {
    if (node == NULL)
        return;
    _post_order_traverse_from_node(node->left, consumer);
    _post_order_traverse_from_node(node->right, consumer);
    consumer(node->key, node->item);
}

static void btree_pre_order_traverse(btree_t *btree, void_key_item_consumer *consumer) {
    struct btree_priv_data *data = (struct btree_priv_data *)btree->priv_data;
    _pre_order_traverse_from_node(data->root, consumer);
}

static void btree_in_order_traverse(btree_t *btree, void_key_item_consumer *consumer) {
    struct btree_priv_data *data = (struct btree_priv_data *)btree->priv_data;
    _in_order_traverse_from_node(data->root, consumer);
}

static void btree_post_order_traverse(btree_t *btree, void_key_item_consumer *consumer) {
    struct btree_priv_data *data = (struct btree_priv_data *)btree->priv_data;
    _post_order_traverse_from_node(data->root, consumer);
}

btree_t *create_btree(struct item_operations *operations) {
    struct btree_priv_data *priv_data = malloc(sizeof(struct btree_priv_data));
    priv_data->ops = operations;
    priv_data->size = 0;
    priv_data->root = NULL;

    btree_t *btree = malloc(sizeof(btree_t));
    btree->priv_data = priv_data;

    btree->empty = btree_empty;
    btree->size = btree_size;
    btree->clear = btree_clear;
    btree->contains = btree_contains;
    btree->get = btree_get;
    btree->set = btree_set;
    btree->delete = btree_delete;
    btree->get_min = btree_get_min;
    btree->get_max = btree_get_max;
    btree->pre_order_traverse = btree_pre_order_traverse;
    btree->in_order_traverse = btree_in_order_traverse;
    btree->post_order_traverse = btree_post_order_traverse;

    return btree;
}

void free_btree(btree_t *btree) {
    struct btree_priv_data *data = (struct btree_priv_data *)btree->priv_data;

    // make sure all items are freed first
    btree_clear(btree);

    // we don't free item operations, we did not allocate it.
    free(data);
    free(btree);
}
