#include <ctypes.h>
#include "data_structs.h"

typedef struct node {
    struct node *prev;
    struct node *next;
    void *value;
} node;

typedef struct queue_data {
    int count;
    struct node *head;
    struct node *tail;
    item_allocator *allocator;
} queue_data;


static bool is_empty(queue *q) {
    return ((queue_data *)q->priv_data)->count == 0;
}

static int length(queue *q) {
    return ((queue_data *)q->priv_data)->count;
}

static void enqueue(queue *q, void *value) {
    queue_data *data = (queue_data *)q->priv_data;

    node *n = data->allocator->allocate(data->allocator);
    n->value = value;
    n->next = NULL;
    n->prev = NULL;

    if (data->tail == NULL) { // the only item
        data->head = n;
        data->tail = n;
    } else {
        data->tail->next = n;
        n->prev = data->tail;
        data->tail = n;
    }
    data->count++;
}

static void *dequeue(queue *q) {
    queue_data *data = (queue_data *)q->priv_data;

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

static void destroy(queue *q) {
    queue_data *data = (queue_data *)q->priv_data;

    // since we shall destroy the item allocator, 
    // we don't need to deallocate each node
    data->allocator->destroy(data->allocator);
    data_structs_free(data);
    data_structs_free(q);
}

queue *create_queue() {
    queue *q = data_structs_malloc(sizeof(queue));
    queue_data *data = data_structs_malloc(sizeof(queue_data));

    data->allocator = create_item_allocator(sizeof(node));
    data->head = NULL;
    data->tail = NULL;
    data->count = 0;

    q->enqueue = enqueue;
    q->dequeue = dequeue;
    q->is_empty = is_empty;
    q->length = length;
    q->priv_data = data;
    q->destroy = destroy;

    return q;
}
