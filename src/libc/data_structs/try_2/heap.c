#include <ctypes.h>
#include <bits.h>
#include "data_structs.h"


typedef struct node {
    struct node *prev;
    struct node *next;
    void *value;
} node;

typedef struct heap_data {
    int count;
    struct node *head;
    struct node *tail;
    item_allocator *allocator;
    comparer *comparer;
} heap_data;


static bool is_empty(heap *h) {
    return ((heap_data *)h->priv_data)->count == 0;
}

static bool length(heap *h) {
    return ((heap_data *)h->priv_data)->count;
}

static void add(heap *h, void *value) {
    heap_data *data = (heap_data *)h->priv_data;

    // we add the item at the tail, then run the heapify process i.e:
    // from the end till the start, check if prev is smaller than ours, if so swap them
    // this wil make sure the largest will float to the head in O(n) time.

    node *n = data->allocator->allocate(data->allocator);
    n->value = value;
    n->next = NULL;
    n->prev = NULL;

    // add it to the tail
    if (data->head == NULL)
        data->head = n;
    if (data->tail != NULL) {
        data->tail->next = n;
        n->prev = data->tail;
    }
    data->tail = n;
    data->count++;

    // we are now running the heapify algorithm, 
    // we travel back from the tail towards head
    // swapping as needed, until we find something with larger value
    // we shall swap the values instead of the nodes, as it's less trouble

    if (data->count >= 2) {
        node *p = data->tail;
        while (p->prev != NULL) {
            int cmp = data->comparer(p->prev->value, p->value);

            // if prev has larger or equal value, we are done.
            if (cmp >= 0)
                break;

            // otherwise, we need to swap and move on
            void *swap = p->value;
            p->value = p->prev->value;
            p->prev->value = swap;

            // now move up
            p = p->prev;
        }
    }
}

static void *extract(heap *h) {
    heap_data *data = (heap_data *)h->priv_data;

    // we extract from the head only
    if (data->head == NULL)
        return NULL;

    node *n = data->head;
    if (n->next == NULL) { // the only item
        data->head = NULL;
        data->tail = NULL;
    } else { // there are other items
        n->next->prev = NULL;
        data->head = n->next;
    }
    data->count--;
    
    void *value = n->value;
    data->allocator->free(data->allocator, n);
    return value;
}

static void destroy(heap *h) {
    heap_data *data = (heap_data *)h->priv_data;

    // since we shall destroy the item allocator, 
    // we don't need to deallocate each node
    data->allocator->destroy(data->allocator);
    data_structs_free(data);
    data_structs_free(h);
}

heap *create_heap(comparer comparer) {
    heap *h = data_structs_malloc(sizeof(heap));
    heap_data *data = data_structs_malloc(sizeof(heap_data));

    data->allocator = create_item_allocator(sizeof(node));
    data->comparer = comparer;
    data->head = NULL;
    data->tail = NULL;
    data->count = 0;

    h->add = add;
    h->extract = extract;
    h->is_empty = is_empty;
    h->length = length;
    h->priv_data = data;
    h->destroy = destroy;

    return h;
}
