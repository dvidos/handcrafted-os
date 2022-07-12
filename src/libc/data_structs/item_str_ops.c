#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "item_ops.h"



static bool str_equals(struct item_operations *ops, void *item, void *target) {
    return strcmp(item, target) == 0;
};

static void *str_clone(struct item_operations *ops, void *item) {
    unsigned char *clone = malloc(strlen(item) + 1);
    strcpy(clone, item);
    return clone;
};

static void str_free(struct item_operations *ops, void *item) {
    free(item);
};

static bool str_matches(struct item_operations *ops, void *item, void *pattern) {
    return strstr(item, pattern) != 0;
}

static int str_compare(struct item_operations *ops, void *item_a, void *item_b) {
    return strcmp(item_a, item_b);
}

static int str_hash(struct item_operations *ops, void *item) {
    int a = 0;
    unsigned char *p = (unsigned char *)item;
    while (*p != '\0') {
        a = (a * 31) + (int)*p; // 31 is a prime
    }
    return a;
}

static void *str_to_string(struct item_operations *ops, void *item) {
    unsigned char *p = malloc(strlen(item) + 1);
    strcpy(p, item);
    return p;
}

struct item_operations *create_str_item_operations() {
    struct item_operations *ops = malloc(sizeof(struct item_operations));
    ops->equals = str_equals;
    ops->clone = str_clone;
    ops->free = str_free;
    ops->compare = str_compare;
    ops->matches = str_matches;
    ops->hash = str_hash;
    ops->to_string = str_to_string;
    ops->priv_data = NULL;
    return ops;
}

void free_str_item_operations(struct item_operations *ops) {
    free(ops);
}

