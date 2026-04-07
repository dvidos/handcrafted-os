#include "../libc_internal.h"

/**
 * @brief Reads a line from a stream.
 *
 * This function reads at most `size - 1` characters from the input `stream`
 * and stores them into the buffer pointed to by `s`. Reading stops after
 * a newline character, at end-of-file, or after `size - 1` characters have
 * been read. A null terminator is always appended.
 *
 * @param s Pointer to the buffer where the line will be stored.
 * @param size The maximum number of characters to read, including the null terminator.
 * @param stream The input stream to read from.
 * @return On success, `s` is returned. On end-of-file or error, NULL is returned.
 *
 * @implNote
 * This function typically reads characters one by one (e.g., using `fgetc`)
 * until a newline, EOF, or buffer limit is reached. It manages buffering
 * and ensures null termination.
 */
// char *fgets(char *s, int size, FILE *stream) {
//     // TODO: Implement fgets for your operating system.
//     // This involves reading a line, managing buffer and newline.
//     (void)s;      // Suppress unused parameter warning
//     (void)size;   // Suppress unused parameter warning
//     (void)stream; // Suppress unused parameter warning
//     errno = ENOSYS; // Function not implemented
//     return NULL;
// }