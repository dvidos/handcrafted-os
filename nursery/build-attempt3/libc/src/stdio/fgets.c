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
 */
char *fgets(char *s, int size, FILE *stream) {
    if (!s || !stream || size <= 0) {
        errno = EINVAL;
        return NULL;
    }

    if (!(stream->flags & _IO_READ)) {
        errno = EBADF; // Stream not open for reading
        stream->flags |= _IO_ERROR;
        return NULL;
    }

    int c = EOF;
    char *p = s;
    size_t count = 0;

    while (count < (size_t)size - 1) {
        c = fgetc(stream);
        if (c == EOF) {
            break;
        }
        *p++ = (char)c;
        count++;
        if (c == '\n') {
            break;
        }
    }

    if (count == 0 && c == EOF) {
        return NULL; // No characters read before EOF or error
    }

    *p = '\0'; // Null-terminate the string
    return s;
}
