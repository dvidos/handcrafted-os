#include "../libc_internal.h"

/**
 * @brief Sets the buffering for a stream to unbuffered or full-buffered.
 *
 * This function sets the buffering mode for the `stream`. If `buf` is NULL,
 * the stream becomes unbuffered. If `buf` is not NULL, it points to a buffer
 * of size `BUFSIZ` which will be used for full buffering.
 *
 * @param stream The `FILE` stream.
 * @param buf A pointer to a character buffer, or NULL.
 *
 * @implNote
 * This function affects the internal state of the `FILE` stream, specifically
 * how it interacts with its internal read/write buffer. It usually needs to
 * be called after `fopen` but before any I/O operations on the stream.
 */
#include "../libc_internal.h"
#include <stdio.h> // For setvbuf, BUFSIZ, _IONBF, _IOFBF

void setbuf(FILE *stream, char *buf) {
    if (buf == NULL) {
        setvbuf(stream, NULL, _IONBF, 0); // No buffering
    } else {
        setvbuf(stream, buf, _IOFBF, BUFSIZ); // Full buffering with user-provided buffer
    }
}