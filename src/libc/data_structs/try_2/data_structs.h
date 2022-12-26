#ifndef _DATA_STRUCTS_H
#define _DATA_STRUCTS_H

#include <ctypes.h>


typedef int comparer(void *item_a, void *item_b);
typedef int hasher(void *item);
typedef void visitor(void *item);
typedef bool matcher(void *item, void *pattern);
typedef bool filterer(void *item);
typedef void *mapper(void *item);
typedef void *reducer(void *item, void *value);

typedef void (debug_printer)(char *fmt, ...);
typedef void (debug_dumper)(void *buffer, unsigned int size, unsigned int initial_address);
typedef void *(memory_allocator)(unsigned int size);
typedef void (memory_freeer)(void *memory);





// we need a bitmapped memory allocator, for many small, same size memory pieces (e.g. 8 bytes)
// essentially, something like a variable array...
typedef struct item_allocator {
    void *(*allocate)(item_allocator *a);
    void (*free)(item_allocator *a, void *address);

    void *data;
    void (*destroy)(item_allocator *a);
} item_allocator;

item_allocator *create_item_allocator(int item_size, int max_items_needed);







// can be used as a priority queue. sorts in O(N)
typedef struct {
    void (*add)(heap *h, void *value);
    void *(*extract)(heap *h); // gets min or max value, depending on the heap
    void *priv_data;
    void (*destroy)(heap *h);
} heap;
heap *create_heap(bool min, comparer *comparer);


// double linked list. operations mostly O(n)
typedef struct {
    bool (*empty)(collection *c);
    int (*size)(collection *c);
    bool (*contains)(collection *c, void *value); // O(n)
    void *(*find)(collection *c, matcher *matcher, void *pattern); // O(n)
    void (*add)(collection *c, void *value);
    bool (*remove)(collection *c, void *value); // O(n) if we give value, O(1) if we give the block
    void (*enqueue)(collection *c, void *value); // allows FIFO ops
    void *(*dequeue)(collection *c);
    void (*push)(collection *c, void *value);    // allows LIFO ops
    void *(*pop)(collection *c);
    void (*for_each)(collection *c, visitor *visitor);
    collection *(*filter)(collection *c, filterer *filterer);
    collection *(*map)(collection *c, mapper *mapper);
    collection *(*sort)(collection *c, comparer *comparer);
    void *(*reduce)(collection *c, reducer *reducer, void *initial_value);
    void (*clear)(collection *c);
    void *priv_data;
    void (*destroy)(collection *c);
} collection;
collection *create_collection();


// ops in O(lg N)
typedef struct {
    bool (*empty)(btree *t);
    int (*size)(btree *t);
    bool (*contains)(btree *t, void *key);
    bool (*add)(btree *t, void *key, void *value);
    bool (*remove)(btree *t, void *key);
    void *(*get)(btree *t, void *key);
    void *(*get_min)(btree *t);
    void *(*get_max)(btree *t);
    void (*clear)(btree *t);
    void *priv_data;
    void (*destroy)(btree *t);
} btree;
btree *create_btree(comparer *comparer);


// ops in O(1)
typedef struct hashtable_s hashtable;
struct hashtable_s {
    bool (*empty)(hashtable *h);
    int (*size)(hashtable *h);
    bool (*contains)(hashtable *h, void *key);
    void (*add)(hashtable *h, void *key, void *value);
    void (*remove)(hashtable *h, void *key);
    void *(*get)(hashtable *h, void *key);
    void (*clear)(hashtable *h);
    void *priv_data;
    void (*destroy)(hashtable *h);
};
hashtable *create_hashtable(int capacity, hasher *hasher);


// ideally we can also have an assoc_array, 
// something between a hashtable and a collection
// it would allow us to have json structures
// that's what we want: a hashtable and collection ability,
// or a n-ary tree, each with a linked list of children...
// to store even the devices or a directory structure etc.

#endif
