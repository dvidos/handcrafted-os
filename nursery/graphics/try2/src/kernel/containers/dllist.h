#pragma once
#include "../fundamentals.h"



// assumes a doubly linked list of structures that have a next/prev pointer
// next points towards tail, and prev points towards head

#define DLL_REMOVE(head, tail, item)            \
    do {                                        \
        if ((item)->prev)                       \
            (item)->prev->next = (item)->next;  \
        else if ((head) == (item))              \
            (head) = (item)->next;              \
                                                \
        if ((item)->next)                       \
            (item)->next->prev = (item)->prev;  \
        else if ((tail) == (item))              \
            (tail) = (item)->prev;              \
                                                \
        (item)->prev = 0;                    \
        (item)->next = 0;                    \
    } while (0)


#define DLL_PUSH_HEAD(head, tail, item)         \
    do {                                        \
        (item)->prev = 0;                    \
        (item)->next = (head);                  \
                                                \
        if (head)                               \
            (head)->prev = (item);              \
        else                                    \
            (tail) = (item);                    \
                                                \
        (head) = (item);                        \
    } while (0)


#define DLL_PUSH_TAIL(head, tail, item)         \
    do {                                        \
        (item)->next = 0;                    \
        (item)->prev = (tail);                  \
                                                \
        if (tail)                               \
            (tail)->next = (item);              \
        else                                    \
            (head) = (item);                    \
                                                \
        (tail) = (item);                        \
    } while (0)


// ---------------------------------------------------------------


typedef bool (*predicate_func)(void *item, void *context);



/*
    Doubly linked list, designed to be embedded in other structures
    Relies on the offset between the structure and the "dlist_node_t" attribute.
    This supports opaque pointers, given an opaque function that returns the offset.
*/

typedef struct dlist_node {
    struct dlist_node *next;
    struct dlist_node *prev;
} dlist_node_t;

typedef struct {
    dlist_node_t *head;
    dlist_node_t *tail;
    int node_offset; // from host structure to dlist_node member
} dlist_t;

static inline void dlist_init(dlist_t *list, int item_node_offset) {
    list->head = NULL;
    list->tail = NULL;
    list->node_offset = item_node_offset;
}

static inline void *dlist_node_ptr_to_item_ptr(dlist_t *list, dlist_node_t *node) {
    return node == NULL ? NULL : ((void *)node) - list->node_offset;
}

static inline dlist_node_t *dlist_item_ptr_to_node_ptr(dlist_t *list, void *item) {
    return item == NULL ? NULL : (dlist_node_t *)(item + list->node_offset);
}

static inline void dlist_append(dlist_t *list, void *item) {
    dlist_node_t *node = dlist_item_ptr_to_node_ptr(list, item);

    node->next = NULL;
    node->prev = NULL;

    if (list->tail == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    }
}

static inline void dlist_prepend(dlist_t *list, void *item) {
    dlist_node_t *node = dlist_item_ptr_to_node_ptr(list, item);

    node->prev = NULL;
    node->next = NULL;

    if (list->head == NULL) {
        list->tail = node;
        list->head = node;
    } else {
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
}

static inline void dlist_remove(dlist_t *list, void *item) {
    dlist_node_t *node = dlist_item_ptr_to_node_ptr(list, item);

    if (node->prev == NULL)
        list->head = node->next;
    else
        node->prev->next = node->next;

    if (node->next == NULL)
        list->tail = node->prev;
    else
        node->next->prev = node->prev;
}

static inline void dlist_move_to_head(dlist_t *list, void *item) {
    dlist_remove(list, item);
    dlist_prepend(list, item);
}

static inline void dlist_move_to_tail(dlist_t *list, void *item) {
    dlist_remove(list, item);
    dlist_append(list, item);
}

static inline void *dlist_first(dlist_t *list) {
    return dlist_node_ptr_to_item_ptr(list, list->head);
}

static inline void *dlist_last(dlist_t *list) {
    return dlist_node_ptr_to_item_ptr(list, list->tail);
}

static inline void *dlist_next(dlist_t *list, void *current) {
    dlist_node_t *curr_n = dlist_item_ptr_to_node_ptr(list, current);
    return curr_n == NULL ? NULL : dlist_node_ptr_to_item_ptr(list, curr_n->next);
}

static inline void *dlist_prev(dlist_t *list, void *current) {
    dlist_node_t *curr_n = dlist_item_ptr_to_node_ptr(list, current);
    return curr_n == NULL ? NULL : dlist_node_ptr_to_item_ptr(list, curr_n->prev);
}

#define dlist_foreach(dlist, item_type, item_ptr)   \
    for (item_type *item_ptr = dlist_first(dlist); item_ptr != NULL; item_ptr = dlist_next(dlist, item_ptr))

#define dlist_foreach_reverse(dlist, item_type, item_ptr)   \
    for (item_type *item_ptr = dlist_last(dlist); item_ptr != NULL; item_ptr = dlist_prev(dlist, item_ptr))

static inline void *dlist_find_first(dlist_t *list, predicate_func predicate, void *context) {
    for (dlist_node_t *n = list->head; n; n = n->next) {
        void *item = dlist_node_ptr_to_item_ptr(list, n);
        if (predicate(item, context))
            return item;
    }
    return NULL;
}

static inline void *dlist_find_last(dlist_t *list, predicate_func predicate, void *context) {
    for (dlist_node_t *n = list->tail; n; n = n->prev) {
        void *item = dlist_node_ptr_to_item_ptr(list, n);
        if (predicate(item, context))
            return item;
    }
    return NULL;
}

static inline bool dlist_any(dlist_t *list, predicate_func predicate, void *context) {
    for (dlist_node_t *n = list->head; n; n = n->next) {
        void *item = dlist_node_ptr_to_item_ptr(list, n);
        if (predicate(item, context))
            return true;
    }
    return false;
}

static inline bool dlist_all(dlist_t *list, predicate_func predicate, void *context) {
    for (dlist_node_t *n = list->head; n; n = n->next) {
        void *item = dlist_node_ptr_to_item_ptr(list, n);
        if (!predicate(item, context))
            return false;
    }
    return true;
}

static inline bool dlist_none(dlist_t *list, predicate_func predicate, void *context) {
    for (dlist_node_t *n = list->head; n; n = n->next) {
        void *item = dlist_node_ptr_to_item_ptr(list, n);
        if (predicate(item, context))
            return false;
    }
    return true;
}
