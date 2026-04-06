#include "../libc_internal.h"
#include <string.h> // For strlen

/**
 * @brief Copies a string.
 *
 * This function copies the null-terminated string pointed to by `src`
 * (including the null terminator) to the buffer pointed to by `dest`.
 * The behavior is undefined if the source and destination buffers overlap.
 *
 * @param dest Pointer to the destination buffer.
 * @param src Pointer to the source string.
 * @return A pointer to the destination buffer `dest`.
 */
char *strcpy(char *dest, const char *src) {
    char *original_dest = dest;
    while ((*dest++ = *src++) != '\0');
    return original_dest;
}