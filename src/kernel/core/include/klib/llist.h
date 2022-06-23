
/**
 * Linked list working macros (templates)
 * They work with any structure with a member named "next".
 * 
 * LL_EMPTY(head)                       // checks if head is empty
 * LL_APPEND(head, item, type)          // adds item at the end of the list
 * LL_PREPEND(head, item, type)         // adds item at start of the list
 * LL_FOREACH(head, ptr, type)          // add a curly braces block
 * LL_FIND(head, ptr, condition, type)  // ptr = either null or target.
 * LL_REMOVE(head, item, type)          // item removed from list
 * LL_COUNT(head, type, counter)        // counter = number of items
 */

#define LL_APPEND(head, item, type)   \
    if (head == NULL) {               \
        head = item;                  \
        item->next = NULL;            \
    } else {                          \
        type _ptr = head->next;       \
        while (_ptr->next != NULL)    \
            _ptr = _ptr->next;        \
        _ptr->next = item;            \
        item->next = NULL;            \
    }


#define LL_PREPEND(head, item, type)   \
    do {                               \
        item->next = head;             \
        head = item;                   \
    } while (false)


#define LL_FOREACH(head, ptr, type)  \
    for (type ptr = head; ptr != NULL; ptr = ptr->next)  // add block statement { ... }


#define LL_FIND(head, ptr, condition)    \
    do {                                 \
        ptr = NULL;                      \
        _found = false;                  \
        for (ptr = head; ptr != NULL; ptr = ptr->next)  \
        LL_FOREACH(head, ptr, type) {    \
            if (codition) {              \
                _found = true;           \
                break;                   \
            }                            \
        }                                \
        if (!_found)                     \
            ptr = NULL;                  \
    } while (false)


#define LL_REMOVE(head, item, type)         \
    do {                                    \
        if (item == head) {                 \
            head = head->next;              \
        } else {                            \
            type _ptr = head;               \
            while (_ptr->next != item)      \
                _ptr = _ptr->next;          \
            _ptr->next = _ptr->next->next;  \
        }                                   \
        item->next = NULL;                  \
    } while (false)


#define LL_COUNT(head, type, counter)   \
    do {                                \
        counter = 0;                    \
        for (type _ptr = head; _ptr != NULL; _ptr = _ptr->next)  \
            counter++;                  \
    } while (false)
