#include <ctypes.h>
#include "data_structs.h"


typedef struct node {
    struct node *left;
    struct node *right;
    struct node *parent;
    void *key;
    void *value;
} node;

typedef struct bintree_data {
    struct node *root;
    int count;
    item_allocator *allocator;
    comparer *key_comparer;
} bintree_data;

static bool bintree_is_empty(bintree *t);
static int bintree_count(bintree *t);
static bool bintree_contains(bintree *t, void *key);
static bool bintree_add(bintree *t, void *key, void *value);
static void *bintree_get(bintree *t, void *key);
static bool bintree_remove(bintree *t, void *key);
static void bintree_clear(bintree *t);
static void *bintree_minimum_key(bintree *t);
static void *bintree_maximum_key(bintree *t);
static void *bintree_previous_key(bintree *t, void *key);
static void *bintree_next_key(bintree *t, void *key);
static void bintree_traverse(bintree *t, bintree_traverse_mode mode, kv_visitor *visitor);
static void bintree_destroy(bintree *t);

bintree *create_bintree(comparer *key_comparer) {
    bintree *t = data_structs_malloc(sizeof(bintree));
    bintree_data *data = data_structs_malloc(sizeof(bintree_data));

    data->allocator = create_item_allocator(sizeof(node));
    data->key_comparer = key_comparer;
    data->root = NULL;
    data->count = 0;

    t->is_empty = bintree_is_empty;
    t->count = bintree_count;
    t->contains = bintree_contains;
    t->add = bintree_add;
    t->get = bintree_get;
    t->remove = bintree_remove;
    t->clear = bintree_clear;
    t->minimum_key = bintree_minimum_key;
    t->maximum_key = bintree_maximum_key;
    t->previous_key = bintree_previous_key;
    t->next_key = bintree_next_key;
    t->traverse = bintree_traverse;
    t->priv_data = data;
    t->destroy = bintree_destroy;

    return t;
}

static inline node* find_node_by_key(bintree_data *data, node *current, void *key) {
    while (current != NULL) {
        int cmp = data->key_comparer(key, current->key);
        if (cmp == 0)
            return current;
        else if (cmp < 0)
            current = current->left;
        else if (cmp > 0)
            current = current->right;
    }
    return NULL;
}

static inline node *get_minimum(node *current) {
    if (current == NULL)
        return NULL;
    while (current->left != NULL)
        current = current->left;
    return current;
}

static inline node *get_maximum(node *current) {
    if (current == NULL)
        return NULL;
    while (current->right != NULL)
        current = current->right;
    return current;
}

static node *get_successor(bintree_data *data, node *current) {
    if (current == NULL)
        return NULL;
    
    // if right subtree exists, we just need it's smallest key
    if (current->right != NULL)
        return get_minimum(current->right);

    // else, find the first parent who has us to his left (we are smaller than him)
    // if we get to root and we still are at the right of it, we are the rightmost node
    node *parent = current->parent;
    while (parent != NULL && current == parent->right) {
        current = parent;
        parent = parent->parent;
    }

    return parent;
}

static node *get_predecessor(bintree_data *data, node *current) {
    if (current == NULL)
        return NULL;
    
    // if right subtree exists, we just need it's smallest key
    if (current->left != NULL)
        return get_maximum(current->right);

    // else, find the first parent who has us to his right (we are larger than him).
    // if we get to root and we still are at the left of it, we are the leftmost node
    node *parent = current->parent;
    while (parent != NULL && current == parent->left) {
        current = parent;
        parent = parent->parent;
    }

    return parent;
}

// replace a node with another. simplifies deletion.
static void replace_node(bintree_data *data, node *to_replace, node *replacement) {

    if (to_replace->parent == NULL) {
        // we don't have a parent, we are root
        data->root = replacement;

    } if (to_replace == to_replace->parent->left) {
        // we are our parent's left child
        to_replace->parent->left = replacement;

    } else if (to_replace == to_replace->parent->right) {
        // we are our parent's right child
        to_replace->parent->right = replacement;
    }

    // make sure the parent pointer is maintained
    if (replacement != NULL)
        replacement->parent = to_replace->parent;
    to_replace->parent = NULL;
}

static bool bintree_is_empty(bintree *t) {
    return ((bintree_data *)t->priv_data)->count == 0;
}

static int bintree_count(bintree *t) {
    return ((bintree_data *)t->priv_data)->count;
}

static bool bintree_contains(bintree *t, void *key) {
    bintree_data *data = (bintree_data *)t->priv_data;
    node *n = find_node_by_key(data, data->root, key);
    return (n != NULL);
}

static bool bintree_add(bintree *t, void *key, void *value) {
    bintree_data *data = (bintree_data *)t->priv_data;

    node *new_node = data->allocator->allocate(data->allocator);
    new_node->key = key;
    new_node->value = value;
    new_node->left = NULL;
    new_node->right = NULL;
    new_node->parent = NULL;
    
    // simple case if tree is empty
    if (data->root == NULL) {
        data->root = new_node;
        data->count++;
        return true;
    }

    // nodes are inserted as leaves, find a good place to append
    node *current = data->root;
    while (true) {
        int cmp = data->key_comparer(key, current->key);
        if (cmp == 0) {
            if (data_structs_warn)
                data_structs_warn("Cannot add value to tree, it already exists");
            data->allocator->free(data->allocator, new_node);
            return false;
        }

        // if less than our key, we care about left subtree
        if (cmp < 0) {
            if (current->left == NULL) {
                current->left = new_node;
                new_node->parent = current;
                data->count++;
                return true;
            }
            // otherwise, go deeper in the tree
            current = current->left;
            continue;
        }

        // if less than our key, we care about left subtree
        if (cmp > 0) {
            if (current->right == NULL) {
                current->right = new_node;
                new_node->parent = current;
                data->count++;
                return true;
            }
            // otherwise, go deeper in the tree
            current = current->right;
            continue;
        }
    }

    if (data_structs_warn)
        data_structs_warn("Somehow, could not add value to tree");
    return false;
}

static void *bintree_get(bintree *t, void *key) {
    bintree_data *data = (bintree_data *)t->priv_data;
    node *n = find_node_by_key(data, data->root, key);
    return n == NULL ? NULL : n->value;
}

static bool bintree_remove(bintree *t, void *key) {
    bintree_data *data = (bintree_data *)t->priv_data;

    node *current = find_node_by_key(data, data->root, key);
    if (current == NULL)
        return false;
    
    // for deletion we have three cases:
    // 1. if node is a leaf, we just remove it from tree
    // 2. if node has a single child, the node is removed and the child takes its place
    // 3. if node has both children, we take its successor to replace it.
    //    note that the successor is the leftmost node in the right subtree, so it's left is definitely NULL.
    //    a. if the successor is the immediate child, it takes the place, and it also inherits the nodes left subtree.
    //    b. if the successor is a deeper child, we shall 
    //       - replace the child with its own right child, and then
    //       - replace the node with the deep child.


    if (current->left == NULL && current->right == NULL) {
        // we can freely remove it
        replace_node(data, current, NULL);

    } else if (current->left == NULL) {
        // we just promote the other child to its place
        replace_node(data, current, current->right);

    } else if (current->right == NULL) {
        // we just promote the other child to its place
        replace_node(data, current, current->left);

    } else {

        // there's two children... find the successor.
        // remember, successor has no left child (we would be it, if there was one)
        node *successor = get_successor(data, current);
        if (successor->parent == current) {
            // immediate child, have that replace current and inherit left subtree
            replace_node(data, current, successor);
            successor->left = current->left;
            successor->left->parent = successor;

        } else {
            // first replace the successor with it's own right child, as it has no left child
            replace_node(data, successor, successor->right);
            // then replace the deleted with the successor
            replace_node(data, current, successor);

            // have the replacement point to the original right subtree
            successor->right = current->right;
            successor->right->parent = successor;
            // have the replacement also inherit the left subtree
            successor->left = current->left;
            successor->left->parent = successor;
        }
    }

    // make sure we housekeep
    data->allocator->free(data->allocator, current);
    data->count--;
    return true;
}

static void deallocate_subtree(bintree_data *data, node *current) {
    if (current == NULL)
        return;
    deallocate_subtree(data, current->left);
    deallocate_subtree(data, current->right);
    data->allocator->free(data->allocator, current);
    data->count--;
}

static void bintree_clear(bintree *t) {
    bintree_data *data = (bintree_data *)t->priv_data;
    deallocate_subtree(data, data->root);
}

static void *bintree_minimum_key(bintree *t) {
    bintree_data *data = (bintree_data *)t->priv_data;
    node *n = get_minimum(data->root);
    return n == NULL ? NULL : n->key;
}

static void *bintree_maximum_key(bintree *t) {
    bintree_data *data = (bintree_data *)t->priv_data;
    node *n = get_maximum(data->root);
    return n == NULL ? NULL : n->key;
}

static void *bintree_next_key(bintree *t, void *key) {
    bintree_data *data = (bintree_data *)t->priv_data;
    node *current = find_node_by_key(data, data->root, key);
    node *successor = get_successor(data, current);
    return successor == NULL ? NULL : successor->key;
}

static void *bintree_previous_key(bintree *t, void *key) {
    bintree_data *data = (bintree_data *)t->priv_data;
    node *current = find_node_by_key(data, data->root, key);
    node *predecessor = get_predecessor(data, current);
    return predecessor == NULL ? NULL : predecessor->key;
}

static void bintree_traverse_from_node(node *n, bintree_traverse_mode mode, kv_visitor *visitor) {
    if (n == NULL)
        return;

    if (mode == PRE_ORDER)
        visitor(n->key, n->value);
    bintree_traverse_from_node(n->left, mode, visitor);
    if (mode == IN_ORDER)
        visitor(n->key, n->value);
    bintree_traverse_from_node(n->right, mode, visitor);
    if (mode == POST_ORDER)
        visitor(n->key, n->value);
}

static void bintree_traverse(bintree *t, bintree_traverse_mode mode, kv_visitor *visitor) {
    bintree_data *data = (bintree_data *)t->priv_data;
    bintree_traverse_from_node(data->root, mode, visitor);
}

static void bintree_destroy(bintree *t) {
    bintree_data *data = (bintree_data *)t->priv_data;

    data->allocator->destroy(data->allocator);
    data_structs_free(data);
    data_structs_free(t);
}

