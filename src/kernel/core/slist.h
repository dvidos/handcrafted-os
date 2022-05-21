#ifndef _SLIST_H
#define _SLIST_H

// maintains a list of strings

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



#endif
