#include <klib/string.h>
#include "collection.h"
#include "item_ops.h"


struct collection_node {
    void *item;
    struct collection_node *next;
    struct collection_node *prev;  
};

struct collection_priv_data {
    struct item_operations *ops;
    int size;
    struct collection_node *iter;
    bool iter_before_head;
    struct collection_node *head;
    struct collection_node *tail;
};

static bool collection_empty(collection_t *collection) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    return data->size == 0;
}

static int collection_size(collection_t *collection) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    return data->size;
}

static void collection_clear(collection_t *collection) {
    // must free everything.
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    struct collection_node *curr = data->head;
    struct collection_node *to_free;
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

static void *collection_get(collection_t *collection, int index) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    struct collection_node *curr = data->head;
    while (index-- > 0 && curr != NULL) {
        curr = curr->next;
    }
    return curr == NULL ? NULL : curr->item;
}

static void collection_append(collection_t *collection, void *item) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    struct collection_node *node = malloc(sizeof(struct collection_node));
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

static void *collection_pop(collection_t *collection) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    struct collection_node *curr;
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

static void collection_enqueue(collection_t *collection, void *item) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    struct collection_node *node = malloc(sizeof(struct collection_node));
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

static void *collection_dequeue(collection_t *collection) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    struct collection_node *curr;
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

static void collection_insert(collection_t *collection, int index, void *item) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    if (index <= 0) {
        collection_enqueue(collection, item);
    } else if (index >= data->size) {
        collection_append(collection, item);
    } else {
        struct collection_node *node = malloc(sizeof(struct collection_node));
        node->item = data->ops->clone(data->ops, item);
        node->prev = NULL;
        node->next = NULL;

        // definitely somewhere in the middle, fix both pointers
        struct collection_node *curr = data->head;
        while (index-- > 0)
            curr = curr->next;

        node->next = curr;
        node->prev = curr->prev;
        node->next->prev = node;
        node->prev->next = node;
    }
}

static void collection_remove(collection_t *collection, int index) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    if (index <= 0) {
        void *item = collection_dequeue(collection);
        data->ops->free(data->ops, item);
    } else if (index >= data->size) {
        void *item = collection_pop(collection);
        data->ops->free(data->ops, item);
    } else {
        // definitely somewhere in the middle
        struct collection_node *curr = data->head;
        while (index-- > 0)
            curr = curr->next;
        curr->next->prev = curr->prev;
        curr->prev->next = curr->next;

        data->ops->free(data->ops, curr->item);
        free(curr);
    }
}

static void collection_append_all(collection_t *collection, struct collection_struct *other) {
    other->iter_reset(other);
    while (other->iter_has_next(other))
        collection_append(collection, other->iter_get_next(other));
}

static void collection_enqueue_all(collection_t *collection, struct collection_struct *other) {
    other->iter_reset(other);
    while (other->iter_has_next(other))
        collection_enqueue(collection, other->iter_get_next(other));
}

static void collection_iter_reset(collection_t *collection) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    // position iterator before the head
    data->iter_before_head = true;
    data->iter = NULL;
}

static bool collection_iter_has_next(collection_t *collection) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    // in theory, we look if the current position can be advanced (to get the next item)
    if (data->iter_before_head) {
        return data->head != NULL;
    } else if (data->iter != NULL) {
        return data->iter->next != NULL;
    } else {
        return false;
    }
}

static void *collection_iter_get_next(collection_t *collection) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
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

static bool collection_contains(collection_t *collection, void *item) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    struct collection_node *curr = data->head;
    while (curr != NULL) {
        if (data->ops->matches(data->ops, curr->item, item))
            return true;
        curr = curr->next;
    }
    return false;
}

static int collection_index_of(collection_t *collection, void *item) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    int index = 0;
    struct collection_node *curr = data->head;
    while (curr != NULL) {
        if (data->ops->matches(data->ops, curr->item, item))
            return index;
        curr = curr->next;
        index++;
    }
    return -1;
}

static int collection_last_index_of(collection_t *collection, void *item) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    int index = data->size - 1;
    struct collection_node *curr = data->tail;
    while (curr != NULL) {
        if (data->ops->matches(data->ops, curr->item, item))
            return index;
        curr = curr->prev;
        index--;
    }
    return -1;
}

static void *collection_get_max(collection_t *collection) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    if (data->size == 0)
        return NULL;

    // now we must traverse and compare
    void *maximum = data->head->item;
    struct collection_node *node = data->head->next;
    while (node != NULL) {
        if (data->ops->compare(data->ops, node->item, maximum) > 0)
            maximum = node->item;
        node = node->next;
    }
    return maximum;
}

static void *collection_get_min(collection_t *collection) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    if (data->size == 0)
        return NULL;

    // now we must traverse and compare
    void *minimum = data->head->item;
    struct collection_node *node = data->head->next;
    while (node != NULL) {
        if (data->ops->compare(data->ops, node->item, minimum) < 0)
            minimum = node->item;
        node = node->next;
    }
    return minimum;
}

static void collection_sort(collection_t *collection, bool ascending) {
    // there's a nice challenge, to implement this once, in a handcoed way
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;

    // IIRC, qsort splits things into two arbitrary blocks, take an arbitrary medium
    // move all item smaller than the medium to one block, move the bigger ones to the other,
    // then bring them together.
}

static void collection_for_each(collection_t *collection, void_ptr_consumer *consumer) {
    struct collection_priv_data *data = (struct collection_priv_data *)collection->priv_data;
    struct collection_node *curr = data->head;
    while (curr != NULL) {
        consumer(curr->item);
        curr = curr->next;
    }
}

collection_t *create_collection(struct item_operations *operations) {
    struct collection_priv_data *data = malloc(sizeof(struct collection_priv_data));
    data->ops = operations;
    data->size = 0;
    data->iter = NULL;
    data->head = NULL;
    data->tail = NULL;

    collection_t *collection = malloc(sizeof(collection_t));
    collection->priv_data = data;

    collection->empty = collection_empty;
    collection->size = collection_size;
    collection->clear = collection_clear;
    collection->get = collection_get;
    collection->insert = collection_insert;
    collection->remove = collection_remove;
    collection->append = collection_append;
    collection->pop = collection_pop;
    collection->enqueue = collection_enqueue;
    collection->dequeue = collection_dequeue;
    collection->append_all = collection_append_all;
    collection->enqueue_all = collection_enqueue_all;
    collection->iter_reset = collection_iter_reset;
    collection->iter_has_next = collection_iter_has_next;
    collection->iter_get_next = collection_iter_get_next;
    collection->contains = collection_contains;
    collection->index_of = collection_index_of;
    collection->last_index_of = collection_last_index_of;
    collection->get_max = collection_get_max;
    collection->get_min = collection_get_min;
    collection->sort = collection_sort;
    collection->for_each = collection_for_each;

    return collection;
}
void free_collection(collection_t *collection) {

    // make sure all items are freed first
    collection_clear(collection);

    // we don't free item operations, we did not allocate it.
    free(collection->priv_data);
    free(collection);
}
