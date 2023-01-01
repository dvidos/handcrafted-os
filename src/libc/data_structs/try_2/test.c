#include <ctypes.h>
#include <stdlib.h>
#include <stdio.h>
#include "data_structs.h"

#define assert(x)    \
    if (!(x))        \
        printf("Assertion failed: \"%s\", at %s:%d\n", #x, __FILE__, __LINE__);
    



void *malloc_stump(uint32_t size) {
    return malloc(size);
}
void free_stump(void *address) {
    free(address);
}


typedef struct node {
    struct node *next;
    int value;
} node;

void test_item_allocator() {
    init_data_structs(malloc_stump, free_stump, NULL);
    item_allocator *alloc = create_item_allocator(sizeof(node));

    int num_pointers = 2 * 128 + 1;
    node **ptrs = malloc(num_pointers * sizeof(node *));
    
    for (int i = 0; i < num_pointers; i++) {
        ptrs[i] = alloc->allocate(alloc);
    }

    // we can see the first 128 are consecutive
    assert(ptrs[1] == ptrs[0] + sizeof(node));
    assert(ptrs[2] == ptrs[1] + sizeof(node));

    // but then it's not consecutive
    assert(ptrs[129] != ptrs[128] + sizeof(node));

    // and then again
    assert(ptrs[130] == ptrs[129] + sizeof(node));
    assert(ptrs[131] == ptrs[130] + sizeof(node));
    
    for (int i = 0; i < num_pointers; i++) {
        alloc->free(alloc, ptrs[i]);
    }

    free(ptrs);
    alloc->destroy(alloc);
}

void test_queue() {
    queue *q = create_queue();

    q->destroy(q);
}

void test_stack() {
    stack *s = create_stack();

    s->destroy(s);
}

static int compare_stump(void *a, void *b) {

}

void test_heap() {
    heap *h = create_heap(compare_stump);

    h->destroy(h);
}
