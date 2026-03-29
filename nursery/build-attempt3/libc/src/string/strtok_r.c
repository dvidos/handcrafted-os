#include <string.h>


char *strtok_r(char *str, const char *delim, char **saveptr) {
    char *token;
    if (str == NULL) {
        str = *saveptr;
    }

    // Skip leading delimiters
    str += strspn(str, delim);
    if (*str == '\0') {
        *saveptr = str;
        return NULL;
    }

    // Find the end of the token
    token = str;
    str = strpbrk(token, delim);
    if (str == NULL) {
        // No more delimiters, this is the last token
        *saveptr = token + strlen(token);
    } else {
        *str = '\0'; // Null-terminate the token
        *saveptr = str + 1; // Start next search after this delimiter
    }

    return token;
}
