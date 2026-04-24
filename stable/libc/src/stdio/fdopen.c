#include "../libc_internal.h"

/**
 * @brief Associates a stream with an existing file descriptor.
 *
 * This function associates a new `FILE` stream with the existing file descriptor `fd`.
 * The `mode` string specifies the type of access requested (e.g., "r", "w", "a").
 *
 * @param fd An existing open file descriptor.
 * @param mode The access mode string for the new stream.
 * @return On success, a pointer to the new `FILE` object is returned. On error, NULL
 *         is returned, and `errno` is set.
 */
FILE *fdopen(int fd, const char *mode) {
    if (fd < 0 || !mode) {
        errno = EINVAL;
        return NULL;
    }

    int internal_flags = 0;
    int requested_access = 0; // O_RDONLY, O_WRONLY, or O_RDWR based on mode

    // Parse mode string to determine internal flags and requested access
    if (strchr(mode, 'r')) {
        internal_flags |= _IO_READ;
        requested_access |= O_RDONLY;
    }
    if (strchr(mode, 'w')) {
        internal_flags |= _IO_WRITE;
        requested_access |= O_WRONLY;
    }
    if (strchr(mode, 'a')) {
        internal_flags |= _IO_WRITE | _IO_APPEND;
        requested_access |= O_WRONLY; // Append implies write
    }
    if (strchr(mode, '+')) {
        internal_flags |= _IO_READ | _IO_WRITE;
        requested_access |= O_RDWR;
    }

    if (strchr(mode, 'b')) {
        internal_flags |= _IO_BINARY;
    }

    // TODO: A more robust fdopen would verify if the actual access mode of `fd`
    //       matches the requested `mode`. For simplicity, we assume they match.

    // Allocate FILE structure
    FILE *stream = (FILE *)malloc(sizeof(FILE));
    if (!stream) {
        errno = ENOMEM;
        return NULL;
    }

    // Allocate buffer
    // TODO: Consider handling _IONBF (unbuffered) case where no buffer is needed.
    stream->buffer = (char *)malloc(BUFSIZ);
    if (!stream->buffer) {
        free(stream);
        errno = ENOMEM;
        return NULL;
    }

    // Initialize FILE structure
    stream->fd = fd;
    stream->buf_size = BUFSIZ;
    stream->pos = 0;
    stream->end = 0;
    stream->flags = internal_flags | _IO_FULL_BUF; // Default to full buffering
    stream->next = __open_files_list; // Add to linked list
    __open_files_list = stream;       // Make it the new head

    return stream;
}