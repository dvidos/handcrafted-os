#include "libc_internal.h"
#include <string.h> // For strlen, strncat

/**
 * @brief Safely concatenates strings, guaranteeing null-termination. (BSD extension)
 *
 * This function appends the null-terminated string `src` to the end of `dst`.
 * It will append at most `dsize - strlen(dst) - 1` characters. It will always
 * null-terminate the result.
 * It is designed to be a safer alternative to `strcat` and `strncat`.
 *
 * @param dst The destination buffer.
 * @param src The source string.
 * @param dsize The size of the destination buffer.
 * @return The total length of the string that would have been created if `dst`
 *         was large enough (i.e., `strlen(dst) + strlen(src)`). If the return
 *         value is >= `dsize`, the output string has been truncated.
 *
 * @implNote
 * This function is a BSD extension and not part of standard C.
 * It first determines the current length of `dst`, then copies characters
 * from `src` up to the remaining space in `dst`, ensuring null termination.
 * The return value helps detect truncation.
 */
size_t strlcat(char *dst, const char *src, size_t dsize) {
    // TODO: Implement strlcat.
    // This is a BSD extension for safe string concatenation.
    (void)dst;   // Suppress unused parameter warning
    (void)src;   // Suppress unused parameter warning
    (void)dsize; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return 0;
}