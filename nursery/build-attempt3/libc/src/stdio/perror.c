#include "libc_internal.h"

/**
 * @brief Prints a system error message to `stderr`.
 *
 * This function prints a message to the standard error stream (`stderr`).
 * If `s` is not NULL, it prints the string `s` followed by a colon and a space,
 * then the system error message corresponding to the current value of `errno`,
 * and finally a newline character.
 *
 * @param s An optional string prefix to the error message.
 *
 * @implNote
 * This function typically uses `fprintf` (or lower-level `write` to `stderr`)
 * and `strerror` to format and output the message. It's useful for debugging
 * and user-friendly error reporting.
 */
void perror(const char *s) {
    // TODO: Implement perror for your operating system.
    // This involves printing a user-provided string and the system error message to stderr.
    (void)s; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented - this function does not return a value
}