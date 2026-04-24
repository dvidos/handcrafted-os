#include "../libc_internal.h"

// Default file permissions when creating a new file
#define DEFAULT_FILE_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) // 0666

/**
 * @brief Opens a file and associates a stream with it.
 *
 * This function opens the file specified by `filename` and associates a `FILE`
 * stream with it. The `mode` string specifies the type of access requested
 * (e.g., "r" for read, "w" for write, "a" for append, with optional "b" for binary).
 *
 * @param filename The path to the file to open.
 * @param mode The access mode string.
 * @return On success, a pointer to the `FILE` object is returned. On error, NULL
 *         is returned, and `errno` is set.
 */
FILE *fopen(const char *filename, const char *mode) {
    int open_flags = 0;
    int access_mode = 0; // O_RDONLY, O_WRONLY, O_RDWR
    mode_t file_perms = 0; // Only used with O_CREAT
    int internal_flags = 0;

    // Parse the mode string
    if (!filename || !mode) {
        errno = EINVAL;
        return NULL;
    }

    // Determine read/write/append access and internal flags
    if (strchr(mode, 'r')) {
        access_mode = O_RDONLY;
        internal_flags |= _IO_READ;
    }
    if (strchr(mode, 'w')) {
        access_mode = O_WRONLY;
        open_flags |= O_CREAT | O_TRUNC;
        internal_flags |= _IO_WRITE;
    }
    if (strchr(mode, 'a')) {
        access_mode = O_WRONLY;
        open_flags |= O_CREAT | O_APPEND;
        internal_flags |= _IO_WRITE | _IO_APPEND;
    }

    // Handle combinations like "r+", "w+", "a+"
    if (strchr(mode, '+')) {
        access_mode = O_RDWR;
        internal_flags |= _IO_READ | _IO_WRITE;
    }

    // Add access_mode to open_flags
    open_flags |= access_mode;

    // Set default permissions if creating a file
    if (open_flags & O_CREAT) {
        file_perms = DEFAULT_FILE_PERMS;
    }

    // Check for binary mode (currently no special handling, just set flag)
    if (strchr(mode, 'b')) {
        internal_flags |= _IO_BINARY;
    }

    // Call the underlying open function
    int fd = open(filename, open_flags, file_perms);
    if (fd < 0) {
        // errno is already set by the underlying open() call
        return NULL;
    }

    // Allocate FILE structure
    FILE *stream = (FILE *)malloc(sizeof(FILE));
    if (!stream) {
        close(fd); // Close the opened fd
        errno = ENOMEM;
        return NULL;
    }

    // Allocate buffer
    stream->buffer = (char *)malloc(BUFSIZ);
    if (!stream->buffer) {
        close(fd);
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
