#include <ctypes.h>
#include "data_structs.h"


typedef struct node {
    struct node *next;
    struct node *prev;
    void *value;
} node;

typedef struct list_data {
    int count;
    struct node *head;
    struct node *tail;
    item_allocator *allocator;
    struct node *iteration_current;
} list_data;


static bool list_is_empty(list *l);
static int list_length(list *l);
static bool list_contains(list *l, void *value);
static int list_index_of(list *l, void *value);
static int list_find(list *l, matcher *matcher, void *pattern);
static void *list_get(list *l, int index);
static void list_add(list *l, void *value);
static bool list_insert(list *l, int index, void *value);
static bool list_set(list *l, int index, void *value);
static bool list_delete(list *l, void *value);
static bool list_remove(list *l, int index);
static void list_clear(list *l);
static void list_for_each(list *l, visitor *visitor);
static void *list_iter_first(list *l);
static bool list_iter_valid(list *l);
static void *list_iter_next(list *l);
static list *list_filter(list *l, filterer *filterer, void *filter_arg);
static list *list_map(list *l, mapper *mapper);
static list *list_sort(list *l, comparer *compare);
static list *list_unique(list *l, comparer *compare);
static void list_reduce(list *l, reducer *reduce, void *reduction_arg);
static void list_destroy(list *l);


list *create_list() {
    list *l = data_structs_malloc(sizeof(list));
    list_data *data = data_structs_malloc(sizeof(list_data));

    data->allocator = create_item_allocator(sizeof(node));
    data->head = NULL;
    data->tail = NULL;
    data->count = 0;
    data->iteration_current = NULL;

    l->is_empty = list_is_empty;
    l->length = list_length;
    l->contains = list_contains;
    l->index_of = list_index_of;
    l->find = list_find;
    l->get = list_get;
    l->add = list_add;
    l->insert = list_insert;
    l->set = list_set;
    l->delete = list_delete;
    l->remove = list_remove;
    l->clear = list_clear;
    l->for_each = list_for_each;
    l->iter_first = list_iter_first;
    l->iter_valid = list_iter_valid;
    l->iter_next = list_iter_next;
    l->filter = list_filter;
    l->map = list_map;
    l->sort = list_sort;
    l->unique = list_unique;
    l->reduce = list_reduce;

    l->priv_data = data;
    l->destroy = list_destroy;

    return l;
}

static node *get_node_by_index(list_data *data, int index) {
    if (index < 0 || index > data->count)
        return NULL;
    
    node *n = data->head;
    while (index > 0) {
        // we need to advance, unless we can't
        if (n == NULL)
            return NULL;

        n = n->next;
        index--;
    }

    return n;
}

static bool list_is_empty(list *l) {
    return ((list_data *)l->priv_data)->count == 0;
}

static int list_length(list *l) {
    return ((list_data *)l->priv_data)->count;
}

static bool list_contains(list *l, void *value) {
    return list_index_of(l, value) != -1;
}

static int list_index_of(list *l, void *value) {
    list_data *data = (list_data *)l->priv_data;

    node *n = data->head;
    int index = 0;
    while (n != NULL) {
        if (n->value == value)
            return index;

        n = n->next;
        index++;
    }
    
    return -1;
}

static int list_find(list *l, matcher *matcher, void *pattern) {
    list_data *data = (list_data *)l->priv_data;

    node *n = data->head;
    int index = 0;
    while (n != NULL) {
        if (matcher(n->value, pattern))
            return index;

        n = n->next;
        index++;
    }
    
    return -1;
}

static void *list_get(list *l, int index) {
    list_data *data = (list_data *)l->priv_data;

    node *n = get_node_by_index(data, index);
    return n == NULL ? NULL : n->value;
}

static void list_add(list *l, void *value) {
    list_data *data = (list_data *)l->priv_data;

    node *n = data->allocator->allocate(data->allocator);
    n->prev = NULL;
    n->next = NULL;
    n->value = value;

    if (data->count == 0) {
        data->head = n;
        data->tail = n;
    } else {
        data->tail->next = n;
        n->prev = data->tail;
        data->tail = n;
    }
}

static bool list_insert(list *l, int index, void *value) {
    list_data *data = (list_data *)l->priv_data;

    // check if out of bounds
    if (index < 0 || index > data->count)
        return false;

    // if adding at the end, call relevant method
    if (index == data->count) {
        list_add(l, value);
        return true;
    }

    node *n = data->allocator->allocate(data->allocator);
    n->next = NULL;
    n->prev = NULL;
    n->value = value;

    if (index == 0) {
        // if adding at start, we don't need to count
        if (data->head == NULL) {
            data->head = n;
            data->tail = n;
        } else {
            n->next = data->head;
            data->head->prev = n;
            data->head = n;
        }

    } else {
        // so we are not adding at the head, nor at the tail, but somewhere in between.
        // find the node at index and add before it.
        node *curr = get_node_by_index(data, index);
        if (curr == NULL || curr->prev == NULL || curr->next == NULL) {
            if (data_structs_warn)
                data_structs_warn("Was expecting an interconnected node!");
            return false; // this shouldn't happen!
        }
        
        n->prev = curr->prev;
        n->next = curr;
        curr->prev->next = n;
        curr->prev = n;
    }

    data->count++;
    return true;
}

static bool list_set(list *l, int index, void *value) {
    list_data *data = (list_data *)l->priv_data;

    node *curr = get_node_by_index(data, index);
    if (curr == NULL)
        return false;

    curr->value = value;
    return true;
}

static bool list_delete(list *l, void *value) {
    int index = list_index_of(l, value);
    if (index == -1)
        return false;
    return list_remove(l, index);
}

static bool list_remove(list *l, int index) {
    list_data *data = (list_data *)l->priv_data;
    
    if (data->count == 0)
        return false;

    node *n = NULL;

    // see if we remove from head, tail, or middle.
    if (index == 0) {
        n = data->head;
        data->head = n->next;
        data->head->prev = NULL;

    } else if (index == data->count - 1) {
        n = data->tail;
        data->tail = n->prev;
        data->tail->next = NULL;

    } else {
        // somewhere in the middle
        n = get_node_by_index(data, index);
        if (n == NULL || n->prev == NULL || n->next == NULL) {
            if (data_structs_warn)
                data_structs_warn("Weird, was expecting a node in the middle");
            return false;
        }
        n->prev->next = n->next;
        n->next->prev = n->prev;
    }

    data->count--;
    data->allocator->free(data->allocator, n);
    return true;
}

static void list_clear(list *l) {
    list_data *data = (list_data *)l->priv_data;

    node *n = data->head;
    while (n != NULL) {
        node *next = n->next;
        data->allocator->free(data->allocator, n);
        n = next;
    }

    data->count = 0;
    data->head = NULL;
    data->tail = NULL;
    data->iteration_current = NULL;
}

static void list_for_each(list *l, visitor *visitor) {
    list_data *data = (list_data *)l->priv_data;

    node *n = data->head;
    while (n != NULL) {
        visitor(n->value);
        n = n->next;
    }
}

static void *list_iter_first(list *l) {
    list_data *data = (list_data *)l->priv_data;

    data->iteration_current = data->head;
    return data->iteration_current == NULL ? NULL : data->iteration_current->value;
}

static bool list_iter_valid(list *l) {
    list_data *data = (list_data *)l->priv_data;

    return data->iteration_current != NULL;
}

static void *list_iter_next(list *l) {
    list_data *data = (list_data *)l->priv_data;

    // we may not have a current item at all
    if (data->iteration_current == NULL)
        return NULL;
    
    data->iteration_current = data->iteration_current->next;
    return data->iteration_current == NULL ? NULL : data->iteration_current->value;
}

static list *list_filter(list *l, filterer *filterer, void *filter_arg) {
    list_data *data = (list_data *)l->priv_data;
    
    list *filtered = create_list();
    node *n = data->head;
    while (n != NULL) {
        if (filterer(n->value, filter_arg))
            filtered->add(filtered, n->value);
        
        n = n->next;
    }

    return filtered;
}

static list *list_map(list *l, mapper *mapper) {
    list_data *data = (list_data *)l->priv_data;
    
    list *mapped = create_list();
    node *n = data->head;
    while (n != NULL) {
        void *result = mapper(n->value);
        mapped->add(mapped, result);

        n = n->next;
    }

    return mapped;
}

static list *list_sort(list *l, comparer *compare) {

    // create new list, using insertion sort.
    list *sorted = create_list();
    list_data *sorted_data = (list_data *)sorted->priv_data;

    for (void *value = list_iter_first(l); list_iter_valid(l); value = list_iter_next(l)) {

        // we need to find the first item in the sorted list that is larger than our candidate value
        // then insert before it, or at the end, if none is found
        node *sorted_node = sorted_data->head;
        int index = 0;
        bool found_larger = false;
        while (sorted_node != NULL) {
            if (compare(sorted_node->value, value) > 0) {
                found_larger = true;
                break;
            }
            sorted_node = sorted_node->next;
            index++;
        }

        // insert or append at the right place
        if (found_larger)
            list_insert(sorted, index, value);
        else
            list_add(sorted, value);
    }

    return sorted;
}

static list *list_unique(list *l, comparer *compare) {
    list_data *data = (list_data *)l->priv_data;
    
    // we need to keep track of sameness, 
    // comparing will make this a O(n^2) algorithm

    list *unique = create_list();

    node *source = data->head;
    while (source != NULL) {
        bool exists = false;
        for (void *value = list_iter_first(unique); list_iter_valid(unique); value = list_iter_next(unique)) {
            if (compare(source->value, value) == 0) {
                exists = true;
                break;
            }
        }

        if (!exists)
            unique->add(unique, source->value);
        
        source = source->next;
    }

    return unique;
}

static void list_reduce(list *l, reducer *reduce, void *reduced_value) {
    list_data *data = (list_data *)l->priv_data;

    node *n = data->head;
    while (n != NULL) {
        reduce(n->value, reduced_value);
        n = n->next;
    }
}

static void list_destroy(list *l) {
    list_data *data = (list_data *)l->priv_data;

    // since we shall be releasing the item_allocator
    // we don't need to release individual nodes
    data->allocator->destroy(data->allocator);

    data_structs_free(data);
    data_structs_free(l);
}
