#pragma once


// assumes a doubly linked list of structures that have a next/prev pointer
// next points towards tail, and prev points towards head

#define DLL_REMOVE(head, tail, item)            \
    do {                                        \
        if ((item)->prev)                       \
            (item)->prev->next = (item)->next;  \
        else if ((head) == (item))              \
            (head) = (item)->next;              \
                                                \
        if ((item)->next)                       \
            (item)->next->prev = (item)->prev;  \
        else if ((tail) == (item))              \
            (tail) = (item)->prev;              \
                                                \
        (item)->prev = 0;                    \
        (item)->next = 0;                    \
    } while (0)


#define DLL_PUSH_HEAD(head, tail, item)         \
    do {                                        \
        (item)->prev = 0;                    \
        (item)->next = (head);                  \
                                                \
        if (head)                               \
            (head)->prev = (item);              \
        else                                    \
            (tail) = (item);                    \
                                                \
        (head) = (item);                        \
    } while (0)


#define DLL_PUSH_TAIL(head, tail, item)         \
    do {                                        \
        (item)->next = 0;                    \
        (item)->prev = (tail);                  \
                                                \
        if (tail)                               \
            (tail)->next = (item);              \
        else                                    \
            (head) = (item);                    \
                                                \
        (tail) = (item);                        \
    } while (0)


