#include "list.h"


bool list_contains(list_t *list, void *item) {
    if (list == NULL || item == NULL) return false;
    list_node_t *node = list->first;
    while (node != NULL) {
        if (list_node_ptr_to_item_ptr(list, node) == item)
            return true;
        node = node->next;
    }
    return false;
}

void *list_find_first(list_t *list, list_predicate predicate, void *context) {
    for (list_node_t *n = list->first; n; n = n->next) {
        void *item = list_node_ptr_to_item_ptr(list, n);
        if (predicate(item, context))
            return item;
    }
    return NULL;
}

void *list_find_last(list_t *list, list_predicate predicate, void *context) {
    for (list_node_t *n = list->last; n; n = n->prev) {
        void *item = list_node_ptr_to_item_ptr(list, n);
        if (predicate(item, context))
            return item;
    }
    return NULL;
}

bool list_any(list_t *list, list_predicate predicate, void *context) {
    for (list_node_t *n = list->first; n; n = n->next) {
        void *item = list_node_ptr_to_item_ptr(list, n);
        if (predicate(item, context))
            return true;
    }
    return false;
}

bool list_all(list_t *list, list_predicate predicate, void *context) {
    for (list_node_t *n = list->first; n; n = n->next) {
        void *item = list_node_ptr_to_item_ptr(list, n);
        if (!predicate(item, context))
            return false;
    }
    return true;
}

bool list_none(list_t *list, list_predicate predicate, void *context) {
    for (list_node_t *n = list->first; n; n = n->next) {
        void *item = list_node_ptr_to_item_ptr(list, n);
        if (predicate(item, context))
            return false;
    }
    return true;
}
