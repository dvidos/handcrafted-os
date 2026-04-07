#include "../libc_internal.h"
#include <string.h> // For internal dependencies, though not used directly in this implementation
#include <stddef.h> // For NULL

char *strpbrk(const char *s, const char *accept) {
    // POSIX defines behavior as undefined if s or accept is a null pointer.
    // We proceed assuming valid inputs as per common libc implementations.

    const char *s_ptr = s;
    while (*s_ptr != '\0') {
        const char *accept_ptr = accept;
        while (*accept_ptr != '\0') {
            if (*s_ptr == *accept_ptr) {
                return (char *)s_ptr; // Found a match
            }
            accept_ptr++;
        }
        s_ptr++;
    }
    return NULL; // No match found
}