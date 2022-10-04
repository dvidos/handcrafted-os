
// some macros inspired by linux.
// assumption is a struct that has a member called "next"


#define ll_append(head, item)    \
    if (head == NULL) {          \
        head = item;             \
        item->next = NULL;       \
    } else {                     \
        typeof(head) p = head;   \
        while (p->next != NULL)  \
            p = p->next;         \
        p->next = item;          \
        item->next = NULL;       \
    }


#define ll_remove(head, item)               \
    if (head == item) {                     \
        head = item->next;                  \
        item->next = NULL;                  \
    } else {                                \
        typeof(head) prev = head;           \
        while (prev->next != item && prev->next != NULL)  \
            prev = prev->next;              \
        if (prev->next != NULL)             \
            prev->next = prev->next->next;  \
        item->next = NULL;                  \
    }


#define ll_foreach(head, ptr)   \
    for (typeof(head) ptr = head; ptr != NULL; ptr = ptr->next)


