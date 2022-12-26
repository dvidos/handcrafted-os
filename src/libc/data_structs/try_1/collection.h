#include <string.h>

/*

The idea of having a module with full encapsulation.
For example, make an advanced variant data structure, or a dictionary,
or a driver or anything really.

The principles are:

- One public structure with function pointers, and a pointer to the first argument.
- The pointed structure for the first argument is unkonwn to caller
- One create_xxxxx() function to allocate, prepare and return the structure
- Pass in a structure with pointers of how to treat the items
- That's it. The only known thing is the factory create() method.

Components:
- A public struct with pointers to functions and a pointer to private data
- A function to initialize and return this public structure
- A function to destroy and free the public structure
- The structure of private data is known toand used by  the pointed functions
- Code completion works wonders, when you di "ptr->"
- 
*/


typedef struct collection_struct collection_t;
typedef void void_ptr_consumer(void *item);

// most operations on the collection take O(n)
struct collection_struct {
    void *priv_data;

    bool (*empty)(collection_t *collection);
    int (*size)(collection_t *collection);
    void (*clear)(collection_t *collection);
    void *(*get)(collection_t *collection, int index);

    void (*append)(collection_t *collection, void *item);
    void *(*pop)(collection_t *collection);
    void (*enqueue)(collection_t *collection, void *item);
    void *(*dequeue)(collection_t *collection);
    void (*insert)(collection_t *collection, int index, void *item);
    void (*remove)(collection_t *collection, int index);

    void (*append_all)(collection_t *collection, struct collection_struct *other);
    void (*enqueue_all)(collection_t *collection, struct collection_struct *other);

    void (*iter_reset)(collection_t *collection);
    bool (*iter_has_next)(collection_t *collection);
    void *(*iter_get_next)(collection_t *collection);

    bool (*contains)(collection_t *collection, void *item);
    int (*index_of)(collection_t *collection, void *item);
    int (*last_index_of)(collection_t *collection, void *item);

    void *(*get_max)(collection_t *collection);
    void *(*get_min)(collection_t *collection);
    void (*sort)(collection_t *collection, bool ascending);

    void (*for_each)(collection_t *collection, void_ptr_consumer *consumer);
    void (*free_collection)(collection_t *collection);
};


collection_t *create_collection(struct item_operations *operations);
void free_collection(collection_t *collection);
