#include <string.h>


// len of str1 that consists of char in str2
int strspn(char *str1, char *str2) {
    if (str1 == NULL)
        return 0;
    
    int len = 0;
    while (*str1 != '\0') {
        if (strchr(str2, *str1) == NULL)
            break;

        len++;
        str1++;
    }

    return len;
}

// len of str1 that consists of char NOT in str2
int strcspn(char *str1, char *str2) {
    if (str1 == NULL)
        return 0;
    
    int len = 0;
    while (*str1 != '\0') {
        if (strchr(str2, *str1) != NULL)
            break;

        len++;
        str1++;
    }

    return len;
}

// find first char in str1 that matches any char in str2
char *strpbrk(char *str1, char *str2) {
    if (str1 == NULL)
        return NULL;

    while (*str1 != '\0') {
        if (strchr(str2, *str1) != NULL)
            return str1;
        str1++;
    }

    return NULL;
}
