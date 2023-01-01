#ifndef _DATA_STRUCTS_H
#define _DATA_STRUCTS_H

#include <ctypes.h>


// data structures are memory heavy
typedef void *(malloc_function)(unsigned int size);
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
typedef bool matcher(void *item, void *pattern);      // see if item matches pattern
typedef bool filterer(void *item, void *filter_arg);  // see if item passes the filter
typedef void *mapper(void *item);                     // map the item to a new item
typedef void reducer(void *item, void *reduced_value);  // use item and value to deduce new value



/**
 * @brief Item Allocator is optimized for many small allocations of same size.
 * It uses bitmaps to keep track of allocations. It can grow indefinitely.
 */
typedef struct item_allocator {
    void *(*allocate)(item_allocator *a);
    void (*free)(item_allocator *a, void *address);

    void *priv_data;
    void (*destroy)(item_allocator *a);
} item_allocator;

/**
 * @brief Create an Item Allocator, suited for lots of small allocations of same size. Uses bitmaps for efficient usage of memory.
 * @param item_size size of each item to be allocated
 */
item_allocator *create_item_allocator(int item_size);



/**
 * @brief Represents a FIFO structure. The first item enqueued is the first to be dequeued
 * Operations in O(1)
 */
typedef struct {
    void (*enqueue)(queue *q, void *value);
    void *(*dequeue)(queue *q);
    bool (*is_empty)(queue *q);
    int (*length)(queue *q);
    
    void *priv_data;
    void (*destroy)(queue *h);
} queue;
queue *create_queue();


/**
 * @brief Represents a LIFO structure. The last item pushed is popped first
 * Operations in O(1)
 */
typedef struct {
    void (*push)(stack *q, void *value);
    void *(*pop)(stack *q);
    bool (*is_empty)(stack *q);
    int (*length)(stack *q);
    
    void *priv_data;
    void (*destroy)(stack *h);
} stack;
stack *create_stack();



/**
 * @brief Can be used as a priority queue. The largest value is always at the head, to be extracted.
 * To extract min values, reverse the sorting comparer.
 */
typedef struct {
    void (*add)(heap *h, void *value);
    void *(*extract)(heap *h); // gets min or max value, depending on the heap
    bool (*is_empty)(queue *q);
    int (*length)(queue *q);

    void *priv_data;
    void (*destroy)(heap *h);
} heap;
heap *create_heap(comparer *comparer);


/**
 * @brief Convenience iterator to be used in a for() loop
 * Similar to a foreach() construct
 * To be used as:
 */
typedef struct {
    void (*reset)(iterator *i);
    bool (*hasValue)(iterator *i);
    void (*next)(iterator *i);
    void *(*current)(iterator *i);
} iterator;


/**
 * @brief Syntactic sugar for local iteration loops
 */
#define list_foreach(list, item)             \
    for(void *(item) = (list)->iter_first(list); \
        (list)->iter_valid(list);              \
        (item) = (list)->iter_next(list))

/**
 * @brief Doubly linked list. Most operations happen in O(n)
 */
typedef struct list {
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
} list;
list *create_list();


// ops in O(lg N)
typedef struct {
    bool (*is_empty)(btree *t);
    int (*length)(btree *t);
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


// ideally we can also have an assoc_array, 
// something between a hashtable and a collection
// it would allow us to have json structures
// that's what we want: a hashtable and collection ability,
// or a n-ary tree, each with a linked list of children...
// to store even the devices or a directory structure etc.

#endif
