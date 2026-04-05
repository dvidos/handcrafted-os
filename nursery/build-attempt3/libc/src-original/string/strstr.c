#include <string.h>


char *strstr(char *haystack, char *needle) {
    // there's ton of room for improvement and complex algorithms here
    // but for now, we just want an implementation that works, even a slow one

    if (needle == NULL || needle[0] == '\0')
        return haystack;

    char *start = haystack;
    unsigned int needle_len = strlen(needle);
    while (*start != '\0') {
        char *pos = strchr(start, needle[0]);
        if (pos == NULL)
            return NULL; // first char not found
        if (strlen(pos) < needle_len)
            return NULL; // not enough haystack to check the needle
        if (memcmp(pos, needle, needle_len) == 0)
            return pos; // the whole needle matched (no terminators)
        start = pos + 1; // skip this match, search next from subsquent byte
    }

    return NULL;
}


