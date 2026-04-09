#include "../libc_internal.h"

/**
 * @brief Writes a string to a specified stream.
 *
 * This function writes the null-terminated string `s` to the specified
 * output `stream`. The null terminator itself is not written.
 *
 * @param s The string to write.
 * @param stream The output stream to write to.
 * @return On success, a non-negative value is returned. On error, `EOF` is returned.
 */
int fputs(const char *s, FILE *stream) {
    if (!s || !stream) {
        errno = EINVAL;
        return EOF;
    }

    while (*s) {
        if (fputc(*s, stream) == EOF) {
            return EOF; // Error occurred during fputc
        }
        s++;
    }
    return 0; // Success (POSIX specifies non-negative on success, 0 is common)
}