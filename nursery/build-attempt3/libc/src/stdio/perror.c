#include "../libc_internal.h"

/**
 * @brief Prints a system error message to `stderr`.
 *
 * This function prints a message to the standard error stream (`stderr`).
 * If `s` is not NULL, it prints the string `s` followed by a colon and a space,
 * then the system error message corresponding to the current value of `errno`,
 * and finally a newline character.
 *
 * @param s An optional string prefix to the error message.
 */
void perror(const char *s) {
    if (s != NULL)
        fprintf(stderr, "%s: ", s);
    fprintf(stderr, "%s\n", strerror(errno));
}