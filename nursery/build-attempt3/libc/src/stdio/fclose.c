#include "../libc_internal.h"

/**
 * @brief Closes a file stream.
 *
 * This function closes the file `stream` and flushes any buffered output data.
 * Any unread buffered input data is discarded. The `FILE` object is freed.
 *
 * @param stream The `FILE` stream to close.
 * @return 0 on success, or `EOF` on error.
 */
int fclose(FILE *stream) {
    if (!stream) {
        errno = EBADF;
        return EOF;
    }

    int ret = 0;

    // Flush any pending output
    if (fflush(stream) == EOF) { // Call public fflush
        ret = EOF; // An error occurred during flush
    }

    // Remove the stream from the __open_files_list
    if (__open_files_list == stream) {
        __open_files_list = stream->next;
    } else {
        FILE *current = __open_files_list;
        while (current != NULL && current->next != stream) {
            current = current->next;
        }
        if (current != NULL) { // Found the previous node
            current->next = stream->next;
        }
    }

    // Free the buffer if it was allocated
    if (stream->buffer) {
        free(stream->buffer);
        stream->buffer = NULL;
    }

    // Close the underlying file descriptor
    if (close(stream->fd) < 0) {
        // If an error occurred during close, and no flush error, set ret to EOF
        if (ret == 0) {
            ret = EOF;
        }
    }

    free(stream);

    return ret;
}
