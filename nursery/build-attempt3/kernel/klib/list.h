#pragma once
#include "../include/ctypes.h"


/*
    Doubly linked list, designed to be embedded in other structures
    Relies on the offset between the structure and the "list_node_t" attribute.
    This supports opaque pointers, given an opaque function that returns the offset.
*/

typedef struct list list_t;
typedef struct list_node list_node_t;
typedef bool (*list_predicate)(void *item, void *context);


struct list {
    list_node_t *first;
    list_node_t *last;
    int count;
    int node_struct_offset; // from host structure to list_node member
};

struct list_node {
    list_node_t *next;
    list_node_t *prev;
};

static inline void list_init(list_t *list, int item_node_offset) {
    list->first = NULL;
    list->last = NULL;
    list->count = 0;
    list->node_struct_offset = item_node_offset;
}

static inline int list_count(list_t *list) {
    return list == NULL ? 0 : list->count;
}

static inline bool list_is_empty(list_t *list) {
    return list == NULL || list->count == 0;
}

static inline void *list_node_ptr_to_item_ptr(list_t *list, list_node_t *node) {
    return node == NULL ? NULL : (((char *)node) - list->node_struct_offset);
}

static inline list_node_t *list_item_ptr_to_node_ptr(list_t *list, void *item) {
    return item == NULL ? NULL : (list_node_t *)(((char *)item) + list->node_struct_offset);
}

static inline void list_append(list_t *list, void *item) {
    list_node_t *node = list_item_ptr_to_node_ptr(list, item);

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

    list->count++;
}

static inline void list_prepend(list_t *list, void *item) {
    list_node_t *node = list_item_ptr_to_node_ptr(list, item);

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

    list->count++;
}

static inline void list_remove_by_node(list_t *list, list_node_t *node) {
    if (list == NULL || node == NULL) return;

    if (node->prev == NULL)
        list->first = node->next;
    else
        node->prev->next = node->next;

    if (node->next == NULL)
        list->last = node->prev;
    else
        node->next->prev = node->prev;
        
    node->next = NULL;
    node->prev = NULL;
    list->count--;
}

static inline void list_remove(list_t *list, void *item) {
    list_remove_by_node(list, list_item_ptr_to_node_ptr(list, item));
}

static inline void list_remove_first(list_t *list) {
    list_remove_by_node(list, list->first);
}

static inline void list_remove_last(list_t *list) {
    list_remove_by_node(list, list->last);
}

static inline void list_move_to_first(list_t *list, void *item) {
    if (item == NULL) return;
    list_remove(list, item);
    list_prepend(list, item);
}

static inline void list_move_to_last(list_t *list, void *item) {
    if (item == NULL) return;
    list_remove(list, item);
    list_append(list, item);
}

static inline void *list_first(list_t *list) {
    return list_node_ptr_to_item_ptr(list, list->first);
}

static inline void *list_last(list_t *list) {
    return list_node_ptr_to_item_ptr(list, list->last);
}

static inline void *list_next(list_t *list, void *current) {
    list_node_t *curr_n = list_item_ptr_to_node_ptr(list, current);
    return curr_n == NULL ? NULL : list_node_ptr_to_item_ptr(list, curr_n->next);
}

static inline void *list_prev(list_t *list, void *current) {
    list_node_t *curr_n = list_item_ptr_to_node_ptr(list, current);
    return curr_n == NULL ? NULL : list_node_ptr_to_item_ptr(list, curr_n->prev);
}

static inline void list_push(list_t *list, void *item) {
    list_append(list, item);
}

static inline void *list_pop(list_t *list) {
    if (list->last == NULL) return NULL;
    void *last = list_node_ptr_to_item_ptr(list, list->last);
    list_remove_last(list);
    return last;
}

static inline void list_enqueue(list_t *list, void *item) {
    list_append(list, item);
}

static inline void *list_dequeue(list_t *list) {
    if (list->first == NULL) return NULL;
    void *first = list_node_ptr_to_item_ptr(list, list->first);
    list_remove_first(list);
    return first;
}

#define list_foreach(list, item_type, item_ptr)   \
    for (item_type *item_ptr = list_first(list); item_ptr != NULL; item_ptr = list_next(list, item_ptr))

#define list_foreach_reverse(list, item_type, item_ptr)   \
    for (item_type *item_ptr = list_last(list); item_ptr != NULL; item_ptr = list_prev(list, item_ptr))

    
bool list_contains(list_t *list, void *item);
void *list_find_first(list_t *list, list_predicate predicate, void *context);
void *list_find_last(list_t *list, list_predicate predicate, void *context);
bool list_any(list_t *list, list_predicate predicate, void *context);
bool list_all(list_t *list, list_predicate predicate, void *context);
bool list_none(list_t *list, list_predicate predicate, void *context);
