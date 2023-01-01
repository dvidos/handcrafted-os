#include <ctypes.h>
#include <bits.h>
#include "data_structs.h"

typedef struct node {
    struct node *next;
    void *value;
} node;

typedef struct stack_data {
    int count;
    struct node *top;
    item_allocator *allocator;
} stack_data;


static bool is_empty(stack *s) {
    return ((stack_data *)s->priv_data)->count == 0;
}

static int length(stack *s) {
    return ((stack_data *)s->priv_data)->count;
}

static void push(stack *s, void *value) {
    stack_data *data = (stack_data *)s->priv_data;

    node *n = data->allocator->allocate(data->allocator);
    n->value = value;
    n->next = NULL;

    if (data->top == NULL) { // the only item
        data->top = n;
    } else {
        n->next = data->top;
        data->top = n;
    }
    data->count++;
}

static void *pop(stack *s) {
    stack_data *data = (stack_data *)s->priv_data;

    if (data->top == NULL)
        return NULL;

    node *n = data->top;
    if (n->next == NULL) { // the only item
        data->top = NULL;
    } else { // there are other items
        data->top = n->next;
    }
    data->count--;
    
    void *value = n->value;
    data->allocator->free(data->allocator, n);
    return value;
}

static void destroy(stack *s) {
    stack_data *data = (stack_data *)s->priv_data;

    // since we shall destroy the item allocator, 
    // we don't need to deallocate each node
    data->allocator->destroy(data->allocator);
    data_structs_free(data);
    data_structs_free(s);
}

stack *create_stack() {
    stack *s = data_structs_malloc(sizeof(stack));
    stack_data *data = data_structs_malloc(sizeof(stack_data));

    data->allocator = create_item_allocator(sizeof(node));
    data->top = NULL;
    data->count = 0;

    s->push = push;
    s->pop = pop;
    s->is_empty = is_empty;
    s->length = length;
    s->priv_data = data;
    s->destroy = destroy;

    return s;
}
