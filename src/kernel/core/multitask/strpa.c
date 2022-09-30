#include <memory/kheap.h>
#include <klib/string.h>
#include <klog.h>

MODULE("STRPA");


char **create_str_ptr_arr() {
    // an array of one slot, with the last NULL element
    char **p = kmalloc(sizeof(char *));
    *p = NULL;
    return p;
}

int count_str_ptr_arr(char **ptr) {
    int count = 0;
    char **p = ptr;
    while (*p != NULL) {
        count++;
        p++;
    }

    return count;
}

char **duplicate_str_ptr_arr(char **ptr) {
    // first count the entries, allocate one entry more for final NULL
    int entries = count_str_ptr_arr(ptr);
    char **duplicate = kmalloc(sizeof(char *) * (entries + 1));

    // clone entries as we go
    char **src = ptr;
    char **dest = duplicate;
    while (*src != NULL) {
        char *clone = kmalloc(strlen(*src) + 1);
        strcpy(clone, *src);
        *dest = clone;

        src++;
        dest++;
    }
    *dest = NULL; // the last entry

    return duplicate;
}

void free_str_ptr_arr(char **ptr) {
    // first free the pointed values
    char **p = ptr;
    while (*p != NULL) {
        kfree(*p);
        p++;
    }

    // now free the array of pointers
    kfree(ptr);
}

void debug_str_ptr_arr(char *varname, char **ptr) {
    char **p = ptr;
    int index = 0;
    while (*p != NULL) {
        klog_debug("%s[%d] -> \"%s\"", varname, index, *p);
        p++;
        index++;
        if (index > 99) {
            klog_debug("... more than 100 entries, stopping");
            return;
        }
    }
    klog_debug("%s[%d] -> (null)", varname, index);
}