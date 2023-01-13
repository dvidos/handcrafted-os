#include "../include/testing.h"
#include <stddef.h>
#include <string.h>
#include "setup.h"
#include "list.h"

static bool list_empty(list_node_t *head);
static void list_add(list_node_t *head, void *key, void *payload);
static void list_append(list_node_t *head, list_node_t *node);
static list_node_t *list_first(list_node_t *head);
static list_node_t *list_find_key(list_node_t *head, const char *key);
static list_node_t *list_find_value(list_node_t *head, const void *value, comparator compare);
static list_node_t *list_extract(list_node_t *head, list_node_t *node);
static void list_remove(list_node_t *head, list_node_t *node, actor payload_cleaner, void *extra_data);
static void list_debug(list_node_t *head);
static void list_free(list_node_t *head, actor payload_cleaner, void *extra_data);

struct list_node_ops list_node_ops = {
    .empty = list_empty,
    .add = list_add,
    .append = list_append,
    .first = list_first,
    .find_key = list_find_key,
    .find_value = list_find_value,
    .extract = list_extract,
    .remove = list_remove,
    .debug = list_debug,
    .free = list_free,
};


list_node_t *create_list() {
    list_node_t *head = testing_framework_setup.malloc(sizeof(list_node_t));
    head->next = head;
    head->prev = head;
    head->payload = NULL;
    head->ops = &list_node_ops;
    return head;
}

static void list_add(list_node_t *head, void *key, void *payload) {
    assert(head != NULL);
    list_node_t *node = testing_framework_setup.malloc(sizeof(list_node_t));
    node->key = key;
    node->payload = payload;
    list_append(head, node);
}

static bool list_empty(list_node_t *head) {
    assert(head != NULL);
    return head->next == head;
}

static void list_append(list_node_t *head, list_node_t *node) {
    assert(head != NULL);
    assert(node != NULL);
    node->next = head;
    node->prev = head->prev;
    head->prev->next = node;
    head->prev = node;
}

static list_node_t *list_find_key(list_node_t *head, const char *key) {
    assert(head != NULL);
    list_node_t *node;
    for (node = head->next; node != head; node = node->next) {
        if (strcmp(node->key, key) == 0)
            return node;
    }
    return NULL;
}

static list_node_t *list_find_value(list_node_t *head, const void *value, comparator compare) {
    assert(head != NULL);
    assert(compare != NULL);
    list_node_t *node;
    for (node = head->next; node != head; node = node->next) {
        if (compare(node->payload, value) == 0)
            return node;
    }
    return NULL;
}

static list_node_t *list_first(list_node_t *head) {
    return head->next == head ? NULL : head->next;
}

static list_node_t *list_extract(list_node_t *head, list_node_t *node) {
    assert(head != NULL);
    assert(node != NULL);
    node->prev->next = node->next;
    node->next->prev = node->prev;
    return node;
}

static void list_remove(list_node_t *head, list_node_t *node, actor payload_cleaner, void *extra_data) {
    assert(head != NULL);
    assert(node != NULL);
    node->prev->next = node->next;
    node->next->prev = node->prev;
    if (payload_cleaner)
        payload_cleaner(node->payload, extra_data);
    testing_framework_setup.free(node);
}

static void list_debug(list_node_t *head) {
    if (head == NULL) {
        testing_framework_setup.printf("head = NULL");
        return;
    }
    for_list (node, head) {
        testing_framework_setup.printf("- key  : %s", node->key);
        log_debug_any_value("  value: ", node->payload);
    }
}

static void list_free(list_node_t *head, actor payload_cleaner, void *extra_data) {
    while (!list_empty(head)) {
        list_remove(head, head->next, payload_cleaner, extra_data);
    }
    testing_framework_setup.free(head);
}

