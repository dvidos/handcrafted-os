#include <memory/kheap.h>
#include <klib/string.h>
#include <klog.h>

MODULE("STRVEC");


int count_strvec(char **strvec) {
    int count = 0;
    char **p = strvec;
    while (*p != NULL) {
        count++;
        p++;
    }
    return count;
}

char **clone_strvec(char **strvec) {
    // first count the entries, allocate one entry more for final NULL
    int entries = count_strvec(strvec);
    char **clone = kmalloc(sizeof(char *) * (entries + 1));

    // clone entries as we go
    char **src = strvec;
    char **dest = clone;
    while (*src != NULL) {
        *dest = kmalloc(strlen(*src) + 1);
        strcpy(*dest, *src);
        src++;
        dest++;
    }
    *dest = NULL; // the last entry
    
    return clone;
}

void free_strvec(char **strvec) {
    // first free the pointed values
    char **p = strvec;
    while (*p != NULL) {
        kfree(*p);
        p++;
    }

    // now free the array of pointers
    kfree(strvec);
}

void debug_strvec(char *varname, char **strvec) {
    char **p = strvec;
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
