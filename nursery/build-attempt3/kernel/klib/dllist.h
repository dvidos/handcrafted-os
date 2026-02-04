#pragma once
#include <ctypes.h>


/*
    Doubly linked list, designed to be embedded in other structures
    Relies on the offset between the structure and the "dllist_node_t" attribute.
    This supports opaque pointers, given an opaque function that returns the offset.
*/

typedef struct dllist dllist_t;
typedef struct dllist_node dllist_node_t;
typedef bool (*dllist_predicate)(void *item, void *context);


struct dllist {
    dllist_node_t *first;
    dllist_node_t *last;
    int node_struct_offset; // from host structure to dllist_node member
};

struct dllist_node {
    dllist_node_t *next;
    dllist_node_t *prev;
};

static inline void dllist_init(dllist_t *list, int item_node_offset) {
    list->first = NULL;
    list->last = NULL;
    list->node_struct_offset = item_node_offset;
}

static inline void *dllist_node_ptr_to_item_ptr(dllist_t *list, dllist_node_t *node) {
    return node == NULL ? NULL : ((void *)node) - list->node_struct_offset;
}

static inline dllist_node_t *dllist_item_ptr_to_node_ptr(dllist_t *list, void *item) {
    return item == NULL ? NULL : (dllist_node_t *)(item + list->node_struct_offset);
}

static inline void dllist_append(dllist_t *list, void *item) {
    dllist_node_t *node = dllist_item_ptr_to_node_ptr(list, item);

    node->next = NULL;
    node->prev = NULL;

    if (list->last == NULL) {
        list->first = node;
        list->last = node;
    } else {
        node->prev = list->last;
        list->last->next = node;
        list->last = node;
    }
}

static inline void dllist_prepend(dllist_t *list, void *item) {
    dllist_node_t *node = dllist_item_ptr_to_node_ptr(list, item);

    node->prev = NULL;
    node->next = NULL;

    if (list->first == NULL) {
        list->last = node;
        list->first = node;
    } else {
        node->next = list->first;
        list->first->prev = node;
        list->first = node;
    }
}

static inline void dllist_remove(dllist_t *list, void *item) {
    if (item == NULL) return;
    dllist_node_t *node = dllist_item_ptr_to_node_ptr(list, item);

    if (node->prev == NULL)
        list->first = node->next;
    else
        node->prev->next = node->next;

    if (node->next == NULL)
        list->last = node->prev;
    else
        node->next->prev = node->prev;
}

static inline void dllist_remove_first(dllist_t *list) {
    if (list->first) dllist_remove(list, list->first);
}

static inline void dllist_remove_last(dllist_t *list) {
    if (list->last) dllist_remove(list, list->last);
}

static inline void dllist_move_to_first(dllist_t *list, void *item) {
    dllist_remove(list, item);
    dllist_prepend(list, item);
}

static inline void dllist_move_to_last(dllist_t *list, void *item) {
    dllist_remove(list, item);
    dllist_append(list, item);
}

static inline void *dllist_first(dllist_t *list) {
    return dllist_node_ptr_to_item_ptr(list, list->first);
}

static inline void *dllist_last(dllist_t *list) {
    return dllist_node_ptr_to_item_ptr(list, list->last);
}

static inline void *dllist_next(dllist_t *list, void *current) {
    dllist_node_t *curr_n = dllist_item_ptr_to_node_ptr(list, current);
    return curr_n == NULL ? NULL : dllist_node_ptr_to_item_ptr(list, curr_n->next);
}

static inline void *dllist_prev(dllist_t *list, void *current) {
    dllist_node_t *curr_n = dllist_item_ptr_to_node_ptr(list, current);
    return curr_n == NULL ? NULL : dllist_node_ptr_to_item_ptr(list, curr_n->prev);
}

#define dllist_foreach(dllist, item_type, item_ptr)   \
    for (item_type *item_ptr = dllist_first(dllist); item_ptr != NULL; item_ptr = dllist_next(dllist, item_ptr))

#define dllist_foreach_reverse(dllist, item_type, item_ptr)   \
    for (item_type *item_ptr = dllist_last(dllist); item_ptr != NULL; item_ptr = dllist_prev(dllist, item_ptr))

static inline void *dllist_find_first(dllist_t *list, dllist_predicate predicate, void *context) {
    for (dllist_node_t *n = list->first; n; n = n->next) {
        void *item = dllist_node_ptr_to_item_ptr(list, n);
        if (predicate(item, context))
            return item;
    }
    return NULL;
}

static inline void *dllist_find_last(dllist_t *list, dllist_predicate predicate, void *context) {
    for (dllist_node_t *n = list->last; n; n = n->prev) {
        void *item = dllist_node_ptr_to_item_ptr(list, n);
        if (predicate(item, context))
            return item;
    }
    return NULL;
}

static inline bool dllist_any(dllist_t *list, dllist_predicate predicate, void *context) {
    for (dllist_node_t *n = list->first; n; n = n->next) {
        void *item = dllist_node_ptr_to_item_ptr(list, n);
        if (predicate(item, context))
            return true;
    }
    return false;
}

static inline bool dllist_all(dllist_t *list, dllist_predicate predicate, void *context) {
    for (dllist_node_t *n = list->first; n; n = n->next) {
        void *item = dllist_node_ptr_to_item_ptr(list, n);
        if (!predicate(item, context))
            return false;
    }
    return true;
}

static inline bool dllist_none(dllist_t *list, dllist_predicate predicate, void *context) {
    for (dllist_node_t *n = list->first; n; n = n->next) {
        void *item = dllist_node_ptr_to_item_ptr(list, n);
        if (predicate(item, context))
            return false;
    }
    return true;
}

static inline int dllist_count(dllist_t *list) {
    int count = 0;
    for (dllist_node_t *n = list->first; n; n = n->next) count++;
    return count;
}
