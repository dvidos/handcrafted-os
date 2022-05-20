#include <stddef.h>
#include <stdbool.h>
#include "memory/kheap.h"
#include "drivers/screen.h"
#include "string.h"


struct linked_list_node {
    struct linked_list_node *prev;
    struct linked_list_node *next;
    void *element;
};
typedef struct linked_list_node linked_list_node_t;

struct linked_list {
    struct linked_list_node *head;
    struct linked_list_node *tail;
    int count;
};
typedef struct linked_list linked_list_t;

typedef void (ll_walker_t)(void *);





linked_list_t *llcreate() {
    linked_list_t *list = (linked_list_t *)kalloc(sizeof(linked_list_t));
    memset(list, 0, sizeof(linked_list_t));
    return list;
}

// free all nodes, optionally elements, and the list as well
void llfree(linked_list_t *list, bool free_elements_too) {
    linked_list_node_t *node = list->head;
    while (node != NULL) {
        if (free_elements_too && node->element != NULL)
            kfree(node->element);
        linked_list_node_t *p = node;
        node = node->next;
        kfree(p);
    }
    kfree(list);
}

// add element to end of list - O(1)
void llappend(linked_list_t *list, void *element) {
    linked_list_node_t *node = (linked_list_node_t *)kalloc(sizeof(linked_list_node_t));
    if (node == NULL)
        panic("Cannot allocate node to add to linked list");
    node->element = element;

    if (list->head == NULL) {
        if (list->tail != NULL)
            panic("List tail is NULL, but head is not");
        list->head = node;
        list->tail = node;
        node->next = NULL;
        node->prev = NULL;
    } else {
        if (list->tail == NULL)
            panic("List tail is not NULL, but head is");
        node->next = NULL;
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    }
    list->count++;
}

// add element to start of list - O(1)
void llprepend(linked_list_t *list, void *element) {
    linked_list_node_t *node = (linked_list_node_t *)kalloc(sizeof(linked_list_node_t));
    if (node == NULL)
        panic("Cannot allocate node to add to linked list");
    node->element = element;

    if (list->head == NULL) {
        if (list->tail != NULL)
            panic("List head is NULL, but tail is not");
        list->head = node;
        list->tail = node;
        node->next = NULL;
        node->prev = NULL;
    } else {
        if (list->tail == NULL)
            panic("List tail is NULL, but head is not");
        node->next = list->head;
        node->prev = NULL;
        list->head->prev = node;
        list->head = node;
    }
    list->count++;
}

// pop element from end to list - O(1)
void *llpop(linked_list_t *list) {
    if (list->head == NULL) {
        if (list->tail != NULL)
            panic("List head is NULL, but tail is not");
        return NULL;
    }

    if (list->tail == NULL)
        panic("List head is not NULL, but tail is");
    linked_list_node_t *node = list->tail;
    if (list->head == list->tail) {
        // there is only this node
        list->head = NULL;
        list->tail = NULL;
    } else {
        // there must be at least 2 items
        list->tail->prev->next = NULL;
        list->tail = list->tail->prev;
    }
    list->count--;

    void *element = node->element;
    kfree(node);
    return element;
}

// extract element from start of list - O(1)
void *lldequeue(linked_list_t *list) {
    if (list->head == NULL) {
        if (list->tail != NULL)
            panic("List head is NULL, but tail is not");
        return NULL;
    }

    if (list->tail == NULL)
        panic("List head is not NULL, but tail is");
    linked_list_node_t *node = list->tail;
    if (list->head == list->tail) {
        // there is only this node
        list->head = NULL;
        list->tail = NULL;
    } else {
        // there must be at least 2 items
        list->head->next->prev = NULL;
        list->head = list->head->next;
    }
    list->count--;

    void *element = node->element;
    kfree(node);
    return element;
}

int llindexof(linked_list_t *list, void *element) {
    return 0;
}

void *llget(linked_list_t *list, int index) {
    return NULL;
}

void llremove(linked_list_t *list, int index) {

}

void llwalk(linked_list_t *list, ll_walker_t *walker) {

}
