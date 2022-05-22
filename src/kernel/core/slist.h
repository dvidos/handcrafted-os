#ifndef _SLIST_H
#define _SLIST_H

// maintains a list of strings
typedef struct slist slist_t;

// ideas for future:
// -slist_pop()
// -slist_dequeue()
// -slist_filter(predicate)
// -slist_join(glue)
// -slist_parse(string, separator)
// -slist_contains(item)
// -slist_clear()
// -slist_map(func)
// etc

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
int slist_indexof_containing(slist_t *list, char *str, int start);

int slist_last_indexof(slist_t *list, char *str, int start);
int slist_last_indexof_prefix(slist_t *list, char *str, int start);
int slist_last_indexof_containing(slist_t *list, char *str, int start);

int slist_count_prefix(slist_t *list, char *prefix);
int slist_count_containing(slist_t *list, char *needle);

#endif
