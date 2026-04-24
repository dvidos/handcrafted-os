#include "../libc_internal.h"
#include <string.h> // Potentially for strlen, though not strictly needed for this direct implementation
#include <stddef.h> // For NULL

// Internal static pointer for strtok state
static char *strtok_static_ptr = NULL;

char *strtok(char *str, const char *delim) {
    char *token_start;
    const char *d;

    // If str is NULL, continue from where we left off.
    // Otherwise, start parsing the new string.
    if (str != NULL) {
        strtok_static_ptr = str;
    } else {
        str = strtok_static_ptr;
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
        strtok_static_ptr = NULL; // Reset state
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
        strtok_static_ptr = str + 1; // Save the position for the next call
    } else {
        // Reached the end of the string, no more tokens after this one
        strtok_static_ptr = NULL;
    }

    return token_start;
}
