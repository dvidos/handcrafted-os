#include <string.h>



// first call str has to have a string, subsequent calls must pass NULL
char *strtok(char *str, char *delimiters) {
    // printf("strtok(\"%s\", \"%s\")\n", str, delimiters);
    static char *next = NULL;

    // if NULL is passed, remember last tokenized pointer
    if (str == NULL) {
        if (next == NULL) // user has not called us with valid string yet
            return NULL;
        str = next;
        // printf("strtok(): Continuing at... [%s]\n", str);
    }

    // skip over delimiters in the start of the string
    // printf("strtok(): Skipping over delimiters [%s]\n", delimiters);
    while (*str != '\0' && strchr(delimiters, *str) != NULL) {
        // not sure this is compliant, but we null out all delimiters
        *str = '\0'; 
        str++;

    }
    if (*str == '\0') {
        return NULL; // we reached the end
    }

    // find the next delimiter, we are on a non-delimiter character
    // printf("strtok(): Start of token is [%c]\n", *str);
    char *end = str;
    while (true) {
        if (*end == '\0') {
            next = NULL;
            return str;
        } else if (strchr(delimiters, *end) != NULL) {
            // we found a delimiter
            // printf("strtok(): Found delimiter [%c]\n", *end);
            *end = '\0';
            next = end + 1; // save for later
            return str;
        }
        end++;
    }
}
