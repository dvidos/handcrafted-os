#ifndef _DATA_STRUCTS_H
#define _DATA_STRUCTS_H

#include <ctypes.h>


// data structures are memory heavy
typedef void *(malloc_function)(size_t size);
typedef void (free_function)(void *memory);
typedef void (printf_function)(char *format, ...);

// public, all data structures rely upon these
extern malloc_function *data_structs_malloc;
extern free_function *data_structs_free;
extern printf_function *data_structs_warn;

// this to be called before any data structures methods
void init_data_structs(malloc_function *malloc_function, free_function *free_function, printf_function *warn);



// these for advanced data structures
typedef int comparer(void *item_a, void *item_b);     // return less, equal, greater than zero
typedef int hasher(void *item);                       // return hash value
typedef void visitor(void *item);                     // process an item
typedef void kv_visitor(void *key, void *value);      // process a key-value pair
typedef bool matcher(void *item, void *pattern);      // see if item matches pattern
typedef bool filterer(void *item, void *filter_arg);  // see if item passes the filter
typedef void *mapper(void *item);                     // map the item to a new item
typedef void reducer(void *item, void *reduced_value);  // use item and value to deduce new value

/**
 * @brief Item Allocator is optimized for many small allocations of same size.
 * It uses bitmaps to keep track of allocations. It can grow indefinitely.
 */
typedef struct item_allocator item_allocator;
struct item_allocator {
    void *(*allocate)(item_allocator *a);
    void (*free)(item_allocator *a, void *address);

    void *priv_data;
    void (*destroy)(item_allocator *a);
};

/**
 * @brief Create an Item Allocator, suited for lots of small allocations of same size. Uses bitmaps for efficient usage of memory.
 * @param item_size size of each item to be allocated
 */
item_allocator *create_item_allocator(int item_size);



/**
 * @brief Represents a FIFO structure. The first item enqueued is the first to be dequeued
 * Operations in O(1)
 */
typedef struct queue queue;
struct queue {
    void (*enqueue)(queue *q, void *value);
    void *(*dequeue)(queue *q);
    bool (*is_empty)(queue *q);
    int (*length)(queue *q);
    
    void *priv_data;
    void (*destroy)(queue *h);
};
queue *create_queue();


/**
 * @brief Represents a LIFO structure. The last item pushed is popped first
 * Operations in O(1)
 */
typedef struct stack stack;
struct stack {
    void (*push)(stack *q, void *value);
    void *(*pop)(stack *q);
    bool (*is_empty)(stack *q);
    int (*length)(stack *q);
    
    void *priv_data;
    void (*destroy)(stack *h);
};
stack *create_stack();



/**
 * @brief Can be used as a priority queue. The largest value is always at the head, to be extracted.
 * To extract min values, reverse the sorting comparer.
 */
typedef struct heap heap;
struct heap {
    void (*add)(heap *h, void *value);
    void *(*extract)(heap *h); // gets min or max value, depending on the heap
    bool (*is_empty)(heap *h);
    int (*length)(heap *h);

    void *priv_data;
    void (*destroy)(heap *h);
};
heap *create_heap(comparer *comparer);


/**
 * @brief Syntactic sugar for local iteration loops. To be used with a braces block.
 */
#define foreach_in_list(list, item_var)             \
    for(void *(item_var) = (list)->iter_first(list); \
        (list)->iter_valid(list);              \
        (item_var) = (list)->iter_next(list))

/**
 * @brief Doubly linked list. Most operations happen in O(n)
 */
typedef struct list list;
struct list {
    bool (*is_empty)(list *l);
    int (*length)(list *l);

    bool (*contains)(list *l, void *value);
    int (*index_of)(list *l, void *value);
    int (*find)(list *l, matcher *matcher, void *pattern);
    void *(*get)(list *l, int index);

    void (*add)(list *l, void *value);
    bool (*insert)(list *l, int index, void *value);
    bool (*set)(list *l, int index, void *value);
    bool (*delete)(list *l, void *value);
    bool (*remove)(list *l, int index);
    void (*clear)(list *l);
    
    void (*for_each)(list *l, visitor *visitor);
    void *(*iter_first)(list *l);
    bool (*iter_valid)(list *l);
    void *(*iter_next)(list *l);

    // Creates new list with only values that pass the filter. O(n)
    list *(*filter)(list *l, filterer *filterer, void *filter_arg);

    // Creates new list with results of the mapper as values. O(n)
    list *(*map)(list *l, mapper *mapper);

    // Creates new sorted list. O(n^2)
    list *(*sort)(list *l, comparer *compare);

    // Creates new list with only unique values. O(n^2)
    list *(*unique)(list *l, comparer *compare);

    // Used to aggregate or group values. Reducer is to update reduced_value in place. O(n)
    void (*reduce)(list *l, reducer *reduce, void *reduced_value);

    void *priv_data;
    void (*destroy)(list *l);
};
list *create_list();


// ops in O(lg N)
// associative structure, no key duplication allowed, all ops + comparison based on key.
typedef enum bintree_traverse_mode { PRE_ORDER, IN_ORDER, POST_ORDER } bintree_traverse_mode;
typedef struct bintree bintree;
struct bintree {
    bool (*is_empty)(bintree *t);
    int (*count)(bintree *t);
    bool (*contains)(bintree *t, void *key);
    bool (*add)(bintree *t, void *key, void *value);
    void *(*get)(bintree *t, void *key); // O(log N)
    bool (*remove)(bintree *t, void *key);
    void (*clear)(bintree *t);

    void *(*minimum_key)(bintree *t); // how to get the minimum key, in order to use successor?
    void *(*maximum_key)(bintree *t);
    void *(*previous_key)(bintree *t, void *key); // if we return the key, a new finding is needed to get the value...
    void *(*next_key)(bintree *t, void *key);
    void (*traverse)(bintree *t, bintree_traverse_mode mode, kv_visitor *visitor);

    void *priv_data;
    void (*destroy)(bintree *t);
};
bintree *create_bintree(comparer *key_comparer);


// ops in O(1)
// associative structure, no key duplication allowed, all ops + comparison based on key.
typedef struct hashtable_s hashtable;
struct hashtable_s {
    bool (*is_empty)(hashtable *h);
    int (*length)(hashtable *h);
    bool (*contains)(hashtable *h, void *key);
    void (*add)(hashtable *h, void *key, void *value);
    void (*remove)(hashtable *h, void *key);
    void *(*get)(hashtable *h, void *key);
    void (*clear)(hashtable *h);

    void *priv_data;
    void (*destroy)(hashtable *h);
};
hashtable *create_hashtable(int capacity, hasher *hasher);



// -----------------------------------------------------------





// ideally we can also have an assoc_array, 
// something between a hashtable and a collection
// it would allow us to have json structures
// that's what we want: a hashtable and collection ability,
// or a n-ary tree, each with a linked list of children...
// to store even the devices or a directory structure etc.

// also, build something for graphs.
// at least to allow BFS and DFS traversal of graph.
// remember, each node has a value and a list of connections to other nodes

// also, sets and maps are different.
// sets have key only. maps have keys and values.
// even more, key duplication is not allowed. multiset and multimap will allow this.
// so instead of hashtable, we could also have a hashset and a hashmap


/*
    C++ std library containiers:

    Name             Stores          Key duplication   Implementation
    ---------------- --------------- ----------------- -----------------
    set              keys            no                binary trees
    multiset         keys            yes               binary trees
    map              keys & values   no                binary trees
    multimap         keys & values   yes               binary trees
    ---------------- --------------- ----------------- -----------------
    hash_set         keys            no                hash table
    hash_multiset    keys            yes               hash table
    hash_map         keys & values   no                hash table
    hash_multimap    keys & values   yes               hash table
*/

// ideally, to make things more performing, we should have a way
// to identify actual nodes in the system, to allow fast operations on them,
// instead of giving the index or key again...

/** For fast operations to data structures inners,
 *  clients can use these reference types.
 *  I think we need to take a look at mixins and container_of() macro
 *  Somehow, we must expose a pointer to an internal node to the callers...
 */
typedef struct {
    void *key;
    void *value;
} kv_node_ref;
typedef struct {
    void *value;
} v_node_ref;

// maybe we can have iterators that utilize a special memento pattern struct
// to walk the values, be them dllist, tree, or hashtable...
// we can have two iterators, a value one and a key/value one...

/**
 * @brief An iterator for going over lists, trees, hashtables, graphs etc
 */
typedef struct v_iterator v_iterator;
struct v_iterator {
    void *(*reset)(v_iterator *i);
    bool (*valid)(v_iterator *i);
    void *(*next)(v_iterator *i);

    void *priv_data;
    void (*destroy)(v_iterator *i);
};
v_iterator *create_v_iterator();

/**
 * @brief A Key/Value iterator for going over mapped values
 */
typedef struct kv_iterator kv_iterator;
struct kv_iterator {
    void (*reset)(kv_iterator *i, void **key, void **value);
    bool (*valid)(kv_iterator *i);
    void (*next)(kv_iterator *i, void **key, void **value);

    void *priv_data;
    void (*destroy)(kv_iterator *i);
};
kv_iterator *create_kv_iterator();

// list O(n), tree O(lgN), hash O(1)
typedef enum container_implementation { DL_LIST, BIN_TREE, HASH_TABLE } container_implementation;

// simple data container, implemented either via tree or hashtable
typedef struct set set;
struct set {
    bool (*is_empty)(set *s);
    int (*length)(set *s);
    bool (*contains)(set *s, void *value);
    bool (*add)(set *s, void *value);
    bool (*remove)(set *s, void *value);
    void (*clear)(set *s);
    void (*for_each)(set *s, visitor *visitor); // need a better way to iterate
};
set *create_set(container_implementation implementation, comparer *comparer);

// associative data container
typedef struct map map;
struct map {
    bool (*is_empty)(map *m);
    int (*length)(map *m);
    bool (*contains)(map *m, void *key);
    void *(*find)(map *m, void *key);
    bool (*add)(map *m, void *key, void *value);
    bool (*remove)(map *m, void *key);
    void (*clear)(map *m);
    void (*for_each)(map *m, kv_visitor *kv_visitor); // need a better way to iterate
};
map *create_map(container_implementation implementation, comparer *key_comparer);


// and then we can go on to create solutions for large problems...
// like a cache, or a key-value storage, etc.
// or graphs, to play with problems


#endif
