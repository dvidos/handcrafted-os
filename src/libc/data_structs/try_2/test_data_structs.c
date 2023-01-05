#include <ctypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "data_structs.h"

static bool assertion_failed;

#define assert(x)                   \
    if (!(x)) do {                  \
        assertion_failed = true;    \
        if (data_structs_warn)      \
            data_structs_warn("Assertion failed: \"%s\", at %s:%d\n", #x, __FILE__, __LINE__);  \
    } while (0)
    

static void *malloc_stump(size_t size) {
    return malloc(size);
}

static void free_stump(void *address) {
    free(address);
}

typedef struct node {
    struct node *next;
    int value;
} node;

static void test_item_allocator() {
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

static void test_queue() {
    queue *q = create_queue();

    // test FIFO logic

    q->destroy(q);
}

static void test_stack() {
    stack *s = create_stack();

    // test LIFO logic

    s->destroy(s);
}

static int heap_compare_stump(void *a, void *b) {
    return 0;
}

static void test_heap() {
    heap *h = create_heap(heap_compare_stump);

    // add various values, make sure we extract them in sorted order, add some at runtime.

    h->destroy(h);
}

static void test_list() {
    list *l = create_list();

    l->destroy(l);
}


int str_comparer(void *item_a, void *item_b) {
    return strcmp((char *)item_a, (char *)item_b);
}

static void test_bin_tree() {
    bintree *t = create_bintree(str_comparer);

    // must test all deletion four scenarios
    
    t->destroy(t);
}


/**
 * @brief Verifies data structures function as expected. Returns false otherwise.
 */
bool test_data_structs(printf_function *error_stream) {

    assertion_failed = false;

    init_data_structs(malloc_stump, free_stump, error_stream);

    test_item_allocator();
    test_queue();
    test_stack();
    test_heap();
    test_list();
    test_bin_tree();

    return !assertion_failed;
}
