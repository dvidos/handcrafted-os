#ifndef _LIST_H
#define _LIST_H

#include <stdbool.h>


typedef void (*actor)(const void *payload, const void *extra_data);     
typedef int (*comparator)(const void *payload1, const void *payload2); // return 0 if equal

typedef struct list_node list_node_t;

struct list_node {
    char *key;
    void *payload;
    struct list_node *next;
    struct list_node *prev;
    struct list_node_ops *ops;
};

struct list_node_ops {
    bool (*empty)(list_node_t *head);
    void (*add)(list_node_t *head, void *key, void *payload);
    void (*append)(list_node_t *head, list_node_t *node);
    list_node_t *(*first)(list_node_t *head);
    list_node_t *(*find_key)(list_node_t *head, const char *key);
    list_node_t *(*find_value)(list_node_t *head, const void *value, comparator compare);
    list_node_t *(*extract)(list_node_t *head, list_node_t *node);
    void (*remove)(list_node_t *head, list_node_t *node, actor payload_cleaner, void *extra_data);
    void (*debug)(list_node_t *head);
    void (*free)(list_node_t *head, actor payload_cleaner, void *extra_data);
};

#define for_list(ptr_name, list)    for (list_node_t *(ptr_name) = list->next; ptr_name != list; ptr_name = ptr_name->next)


list_node_t *create_list();




#endif
