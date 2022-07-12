#include <stdbool.h>
#include <stdint.h>
#include <klib/string.h>
#include "list.h"
#include "item_ops.h"


struct list_node {
    void *item;
    struct list_node *next;
    struct list_node *prev;  
};

struct list_priv_data {
    struct item_operations *ops;
    int size;
    struct list_node *iter;
    bool iter_before_head;
    struct list_node *head;
    struct list_node *tail;
};

static bool list_empty(list_t *list) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    return data->size == 0;
}

static int list_size(list_t *list) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    return data->size;
}

static void list_clear(list_t *list) {
    // must free everything.
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    struct list_node *curr = data->head;
    struct list_node *to_free;
    while (curr != NULL) {
        data->ops->free(data->ops, curr->item);
        to_free = curr;
        curr = curr->next;
        free(to_free);
    }
    data->size = 0;
    data->head = NULL;
    data->tail = NULL;
    data->iter = NULL;
}

static void *list_get(list_t *list, int index) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    struct list_node *curr = data->head;
    while (index-- > 0 && curr != NULL) {
        curr = curr->next;
    }
    return curr == NULL ? NULL : curr->item;
}

static void list_append(list_t *list, void *item) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    struct list_node *node = malloc(sizeof(struct list_node));
    node->item = data->ops->clone(data->ops, item);
    node->prev = NULL;
    node->next = NULL;
    if (data->size == 0) {
        data->head = node;
        data->tail = node;
    } else {
        data->tail->next = node;
        node->prev = data->tail;
        data->tail = node;
    }
    data->size++;
}

static void *list_pop(list_t *list) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    struct list_node *curr;
    void *item = NULL;
    if (data->tail == NULL) {
        return NULL;
    } else if (data->size == 1) {
        // removing the only one remaining
        curr = data->tail;
        data->tail = NULL;
        data->head = NULL;
        item = curr->item; // we don't free the item, the caller will
        free(curr);
        data->size = 0;
    } else {
        // definitely more than one items
        curr = data->tail;
        data->tail = curr->prev;
        data->tail->next = NULL;
        item = curr->item; // we don't free the item, the caller will
        free(curr);
        data->size--;
    }

    return item;
}

static void list_enqueue(list_t *list, void *item) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    struct list_node *node = malloc(sizeof(struct list_node));
    node->item = data->ops->clone(data->ops, item);
    node->prev = NULL;
    node->next = NULL;
    if (data->size == 0) {
        data->head = node;
        data->tail = node;
    } else {
        data->head->prev = node;
        node->next = data->head;
        data->head = node;
    }
    data->size++;
}

static void *list_dequeue(list_t *list) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    struct list_node *curr;
    void *item = NULL;
    if (data->head == NULL) {
        return NULL;
    } else if (data->size == 1) {
        // removing the only one remaining
        curr = data->head;
        data->head = NULL;
        data->tail = NULL;
        item = curr->item; // we don't free the item, the caller will
        free(curr);
        data->size = 0;
    } else {
        // definitely more than one items
        curr = data->head;
        data->head = curr->next;
        data->head->prev = NULL;
        item = curr->item; // we don't free the item, the caller will
        free(curr);
        data->size--;
    }

    return item;
}

static void list_insert(list_t *list, int index, void *item) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    if (index <= 0) {
        list_enqueue(list, item);
    } else if (index >= data->size) {
        list_append(list, item);
    } else {
        struct list_node *node = malloc(sizeof(struct list_node));
        node->item = data->ops->clone(data->ops, item);
        node->prev = NULL;
        node->next = NULL;

        // definitely somewhere in the middle, fix both pointers
        struct list_node *curr = data->head;
        while (index-- > 0)
            curr = curr->next;

        node->next = curr;
        node->prev = curr->prev;
        node->next->prev = node;
        node->prev->next = node;
    }
}

static void list_remove(list_t *list, int index) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    if (index <= 0) {
        void *item = list_dequeue(list);
        data->ops->free(data->ops, item);
    } else if (index >= data->size) {
        void *item = list_pop(list);
        data->ops->free(data->ops, item);
    } else {
        // definitely somewhere in the middle
        struct list_node *curr = data->head;
        while (index-- > 0)
            curr = curr->next;
        curr->next->prev = curr->prev;
        curr->prev->next = curr->next;

        data->ops->free(data->ops, curr->item);
        free(curr);
    }
}

static void list_append_all(list_t *list, struct list_struct *other) {
    other->iter_reset(other);
    while (other->iter_has_next(other))
        list_append(list, other->iter_get_next(other));
}

static void list_enqueue_all(list_t *list, struct list_struct *other) {
    other->iter_reset(other);
    while (other->iter_has_next(other))
        list_enqueue(list, other->iter_get_next(other));
}

static void list_iter_reset(list_t *list) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    // position iterator before the head
    data->iter_before_head = true;
    data->iter = NULL;
}

static bool list_iter_has_next(list_t *list) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    // in theory, we look if the current position can be advanced (to get the next item)
    if (data->iter_before_head) {
        return data->head != NULL;
    } else if (data->iter != NULL) {
        return data->iter->next != NULL;
    } else {
        return false;
    }
}

static void *list_iter_get_next(list_t *list) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    void *item = NULL;
    // in theory, first we advance, next the grab
    if (data->iter_before_head) {
        data->iter = data->head;
        item = data->iter->item;
    } else if (data->iter != NULL) {
        data->iter = data->iter->next;
        item = data->iter->item;
    }

    return item;
}

static bool list_contains(list_t *list, void *item) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    struct list_node *curr = data->head;
    while (curr != NULL) {
        if (data->ops->matches(data->ops, curr->item, item))
            return true;
        curr = curr->next;
    }
    return false;
}

static int list_index_of(list_t *list, void *item) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    int index = 0;
    struct list_node *curr = data->head;
    while (curr != NULL) {
        if (data->ops->matches(data->ops, curr->item, item))
            return index;
        curr = curr->next;
        index++;
    }
    return -1;
}

static int list_last_index_of(list_t *list, void *item) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    int index = data->size - 1;
    struct list_node *curr = data->tail;
    while (curr != NULL) {
        if (data->ops->matches(data->ops, curr->item, item))
            return index;
        curr = curr->prev;
        index--;
    }
    return -1;
}

static void *list_get_max(list_t *list) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    if (data->size == 0)
        return NULL;

    // now we must traverse and compare
    void *maximum = data->head->item;
    struct list_node *node = data->head->next;
    while (node != NULL) {
        if (data->ops->compare(data->ops, node->item, maximum) > 0)
            maximum = node->item;
        node = node->next;
    }
    return maximum;
}

static void *list_get_min(list_t *list) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    if (data->size == 0)
        return NULL;

    // now we must traverse and compare
    void *minimum = data->head->item;
    struct list_node *node = data->head->next;
    while (node != NULL) {
        if (data->ops->compare(data->ops, node->item, minimum) < 0)
            minimum = node->item;
        node = node->next;
    }
    return minimum;
}

static void list_sort(list_t *list, bool ascending) {
    // there's a nice challenge, to implement this once, in a handcoed way
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;

    // IIRC, qsort splits things into two arbitrary blocks, take an arbitrary medium
    // move all item smaller than the medium to one block, move the bigger ones to the other,
    // then bring them together.
}

static void list_for_each(list_t *list, void_ptr_consumer *consumer) {
    struct list_priv_data *data = (struct list_priv_data *)list->priv_data;
    struct list_node *curr = data->head;
    while (curr != NULL) {
        consumer(curr->item);
        curr = curr->next;
    }
}

list_t *create_list(struct item_operations *operations) {
    struct list_priv_data *data = malloc(sizeof(struct list_priv_data));
    data->ops = operations;
    data->size = 0;
    data->iter = NULL;
    data->head = NULL;
    data->tail = NULL;

    list_t *list = malloc(sizeof(list_t));
    list->priv_data = data;

    list->empty = list_empty;
    list->size = list_size;
    list->clear = list_clear;
    list->get = list_get;
    list->insert = list_insert;
    list->remove = list_remove;
    list->append = list_append;
    list->pop = list_pop;
    list->enqueue = list_enqueue;
    list->dequeue = list_dequeue;
    list->append_all = list_append_all;
    list->enqueue_all = list_enqueue_all;
    list->iter_reset = list_iter_reset;
    list->iter_has_next = list_iter_has_next;
    list->iter_get_next = list_iter_get_next;
    list->contains = list_contains;
    list->index_of = list_index_of;
    list->last_index_of = list_last_index_of;
    list->get_max = list_get_max;
    list->get_min = list_get_min;
    list->sort = list_sort;
    list->for_each = list_for_each;

    return list;
}
void free_list(list_t *list) {

    // make sure all items are freed first
    list_clear(list);

    // we don't free item operations, we did not allocate it.
    free(list->priv_data);
    free(list);
}
