#include <stdbool.h>
#include <stdint.h>
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


typedef struct list_struct list_t;
typedef void void_ptr_consumer(void *item);

// most operations on the list take O(n)
struct list_struct {
    void *priv_data;

    bool (*empty)(list_t *list);
    int (*size)(list_t *list);
    void (*clear)(list_t *list);
    void *(*get)(list_t *list, int index);

    void (*append)(list_t *list, void *item);
    void *(*pop)(list_t *list);
    void (*enqueue)(list_t *list, void *item);
    void *(*dequeue)(list_t *list);
    void (*insert)(list_t *list, int index, void *item);
    void (*remove)(list_t *list, int index);

    void (*append_all)(list_t *list, struct list_struct *other);
    void (*enqueue_all)(list_t *list, struct list_struct *other);

    void (*iter_reset)(list_t *list);
    bool (*iter_has_next)(list_t *list);
    void *(*iter_get_next)(list_t *list);

    bool (*contains)(list_t *list, void *item);
    int (*index_of)(list_t *list, void *item);
    int (*last_index_of)(list_t *list, void *item);

    void *(*get_max)(list_t *list);
    void *(*get_min)(list_t *list);
    void (*sort)(list_t *list, bool ascending);

    void (*for_each)(list_t *list, void_ptr_consumer *consumer);
    void (*free_list)(list_t *list);
};


list_t *create_list(struct item_operations *operations);
void free_list(list_t *list);
