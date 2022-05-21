#include <stdbool.h>
#include "string.h"
#include "memory/kheap.h"



struct slist_entry {
    struct slist_entry *prev;
    struct slist_entry *next;
    char *str;
};
typedef struct slist_entry slist_entry_t;

typedef struct {
    slist_entry_t *head;
    slist_entry_t *tail;
    int size;
} slist_t;


// maintains a list of strings
slist_t *slist_create();
void slist_free(slist_t *list);
bool slist_append(slist_t *list, char *str);
bool slist_prepend(slist_t *list, char *str);
int slist_size(slist_t *list);
bool slist_empty(slist_t *list);
char *slist_get(slist_t *list, int index);
bool slist_delete_at(slist_t *list, int index);
int slist_indexof(slist_t *list, char *str, int start);
int slist_indexof_prefix(slist_t *list, char *str, int start);


typedef int (*slist_func)(char *entry_str, char *target_str);

static slist_entry_t *create_entry(char *str);
static slist_entry_t *get_entry_at_index(slist_t *list, int index);
static int find_index_using(slist_t *list, char *str, int start, slist_func comparator);
static int string_comparator(char *entry_str, char *target_str);
static int prefix_matcher(char *entry_str, char *target_str);



// simplest list that can hold strings?
slist_t *slist_create() {
    slist_t *list = (slist_t *)kalloc(sizeof(slist_t));
    memset(list, 0, sizeof(slist_t));
    return list;
}

static slist_entry_t *create_entry(char *str) {
    slist_entry_t *entry = (slist_entry_t *)kalloc(sizeof(slist_entry_t));
    entry->str = (char *)kalloc(strlen(str) + 1);
    strcpy(entry->str, str);
    return entry;
}

void slist_free(slist_t *list) {
    slist_entry_t *curr = list->head;
    slist_entry_t *next;
    while (curr != NULL) {
        kfree(curr->str);
        next = curr->next;
        kfree(curr);
        curr = next;
    }
    kfree(list);
}

bool slist_append(slist_t *list, char *str) {
    slist_entry_t *entry = create_entry(str);
    if (list->size == 0) {
        list->head = entry;
        list->tail = entry;
        entry->next = NULL;
        entry->prev = NULL;
    } else {
        entry->next = NULL;
        list->tail->next = entry;
        entry->prev = list->tail;
        list->tail = entry;
    }
    list->size++;
    return true;
}

bool slist_prepend(slist_t *list, char *str) {
    slist_entry_t *entry = create_entry(str);
    if (list->size == 0) {
        list->head = entry;
        list->tail = entry;
        entry->next = NULL;
        entry->prev = NULL;
    } else {
        entry->prev = NULL;
        list->head->prev = entry;
        entry->next = list->head;
        list->head = entry;
    }
    list->size++;
    return true;
}

int slist_size(slist_t *list) {
    return list->size;
}

bool slist_empty(slist_t *list) {
    return list->size == 0;
}

static slist_entry_t *get_entry_at_index(slist_t *list, int index) {
    slist_entry_t *entry = list->head;
    while (entry != NULL && index > 0) {
        entry = entry->next;
        index--;
    }
    return entry;
}

// returns the pointer, does not copy the result anywhere
char *slist_get(slist_t *list, int index) {
    slist_entry_t *entry = get_entry_at_index(list, index);
    return (entry == NULL) ? NULL : entry->str;
}

// deletes and frees the entry
bool slist_delete_at(slist_t *list, int index) {

    slist_entry_t *entry = get_entry_at_index(list, index);
    if (entry == NULL)
        return false;

    if (entry == list->head && entry == list->tail) {
        // if both head and tail point to entry, 
        // this is the only entry in the list
        list->head = NULL;
        list->tail = NULL;
    } else if (entry == list->head) {
        // we must have two or more items
        list->head = list->head->next;
        list->head->prev = NULL;
    } else if (entry == list->tail) {
        // we must have two or more items
        list->tail = list->tail->prev;
        list->tail->next = NULL;
    } else {
        // if neither head nor tail point to the list, we have more than two items
        // and entry is somewhere in the middle
        entry->prev->next = entry->next;
        entry->next->prev = entry->prev;
    }

    kfree(entry->str);
    kfree(entry);
    list->size--;
    return true;
}

static int find_index_using(slist_t *list, char *str, int start, slist_func comparator) {
    // get to starting entry
    slist_entry_t *entry = list->head;
    int pos = 0;
    while (entry != NULL && pos < start) {
        entry = entry->next;
        pos++;
    }
    if (entry == NULL)
        return -1;
    
    // entry points to the first element we should search
    while (entry != NULL) {
        if (comparator(entry->str, str) == 0)
            return pos;
        entry = entry->next;
        pos++;
    }

    return (entry == NULL) ? -1 : pos;
}

static int string_comparator(char *entry_str, char *target_str) {
    return strcmp(entry_str, target_str);
}
static int prefix_matcher(char *entry_str, char *target_str) {
    return memcmp(entry_str, target_str, strlen(target_str));
}

int slist_indexof(slist_t *list, char *str, int start) {
    return find_index_using(list, str, start, string_comparator);
}

int slist_indexof_prefix(slist_t *list, char *prefix, int start) {
    return find_index_using(list, prefix, start, prefix_matcher);
}

