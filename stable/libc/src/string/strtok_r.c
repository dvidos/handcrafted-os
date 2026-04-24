#include "../libc_internal.h"
#include <string.h> // Potentially for strlen, though not strictly needed for this direct implementation
#include <stddef.h> // For NULL

char *strtok_r(char *str, const char *delim, char **saveptr) {
    char *token_start;
    const char *d;

    // If str is NULL, continue from where *saveptr points.
    // Otherwise, start parsing the new string and update *saveptr.
    if (str != NULL) {
        *saveptr = str;
    } else {
        str = *saveptr;
    }

    // If there's no string to parse, or we've reached the end, return NULL.
    if (str == NULL || *str == '\0') {
        return NULL;
    }

    // Skip leading delimiters
    while (*str != '\0') {
        int is_delim = 0;
        d = delim;
        while (*d != '\0') {
            if (*str == *d) {
                is_delim = 1;
                break;
            }
            d++;
        }
        if (!is_delim) {
            break; // Found a non-delimiter character, start of a token
        }
        str++;
    }

    // If we skipped all characters, there are no more tokens
    if (*str == '\0') {
        *saveptr = NULL; // Reset state
        return NULL;
    }

    token_start = str; // Mark the beginning of the token

    // Find the end of the token
    while (*str != '\0') {
        int is_delim = 0;
        d = delim;
        while (*d != '\0') {
            if (*str == *d) {
                is_delim = 1;
                break;
            }
            d++;
        }
        if (is_delim) {
            break; // Found a delimiter, end of the token
        }
        str++;
    }

    // If a delimiter was found, null-terminate the token and save the next starting point.
    if (*str != '\0') {
        *str = '\0'; // Null-terminate the token
        *saveptr = str + 1; // Save the position for the next call
    } else {
        // Reached the end of the string, no more tokens after this one
        *saveptr = NULL;
    }

    return token_start;
}
