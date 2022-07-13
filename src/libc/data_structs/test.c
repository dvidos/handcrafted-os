#include <string.h>
#include "item_ops.h"
#include "list.h"


void test_str_operations() {
    struct item_operations *ops = create_string_item_operations();
    
    // clone
    // free
    // compare
    // matches
    // equals
    // hash
    // to_string

    unsigned char *s = ops->clone(ops, "testing");
    unsigned char *s = ops->clone(ops, "testing");

}

void test_list() {
    // test each if branch of each method.

    // empty
    // size
    // clear
    // get
    // insert
    // remove
    // append
    // pop
    // enqueue
    // dequeue
    // append_all
    // enqueue_all
    // iter_reset
    // iter_has_next
    // iter_get_next
    // contains
    // index_of
    // last_index_of
    // get_max
    // get_min
    // sort
    // for_each

    struct item_operations *str_ops = create_string_item_operations();
    list_t *p = create_list(str_ops);
    p->append(p, "Item 1");
    p->append(p, "Item 2");
    p->append(p, "Item 3");
    while (!p->empty(p)) {
        p->remove(p, 0);
    }

    for (int i = 0; i < p->size(p); i++) {
        void *item = p->get(p, i);
    }

    p->iter_reset(p);
    while (p->iter_has_next(p)) {
        void *item = p->iter_get_next(p);
    }
}

