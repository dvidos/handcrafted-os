#include <string.h>
#include <stdlib.h>

// caller is supposed to free the duplicate
char *strdup(const char *str) {
    if (str == NULL)
        return NULL;
    char *dup = malloc(strlen(str) + 1);
    strcpy(dup, str);
    return dup;
}

