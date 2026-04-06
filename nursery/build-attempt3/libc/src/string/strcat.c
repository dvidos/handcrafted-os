#include "../libc_internal.h"
#include <string.h> // For strlen

/**
 * @brief Appends one string to another.
 *
 * This function appends the null-terminated string pointed to by `src`
 * to the end of the null-terminated string pointed to by `dest`.
 * The first character of `src` overwrites the null terminator of `dest`.
 * The behavior is undefined if the source and destination buffers overlap.
 *
 * @param dest Pointer to the destination buffer.
 * @param src Pointer to the source string.
 * @return A pointer to the destination buffer `dest`.
 */
char *strcat(char *dest, const char *src) {
    char *original_dest = dest;
    while (*dest != '\0') {
        dest++;
    }
    while ((*dest++ = *src++) != '\0');
    return original_dest;
}