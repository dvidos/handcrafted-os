#include "../libc_internal.h"
#include <stddef.h> // For size_t, NULL
// #include <errno.h> // Removed, as not needed for this implementation

char *strstr(const char *haystack, const char *needle) {
    if (!haystack || !needle) {
        // POSIX defines behavior as undefined if haystack or needle is a null pointer.
        // We proceed assuming valid non-NULL inputs as per common libc implementations.
        return NULL;
    }

    if (*needle == '\0') {
        return (char *)haystack; // Empty needle, return haystack
    }

    const char *h_ptr = haystack;
    while (*h_ptr != '\0') {
        const char *n_ptr = needle;
        const char *h_current = h_ptr;

        // Check if needle matches starting from current position in haystack
        while (*n_ptr != '\0' && *h_current != '\0' && *n_ptr == *h_current) {
            n_ptr++;
            h_current++;
        }

        if (*n_ptr == '\0') {
            return (char *)h_ptr; // Needle found
        }
        h_ptr++;
    }

    return NULL; // Needle not found
}