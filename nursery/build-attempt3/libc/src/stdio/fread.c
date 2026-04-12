#include "../libc_internal.h"

/**
 * @brief Reads data from a stream into a buffer.
 *
 * This function reads `nmemb` elements of `size` bytes each from the input `stream`
 * and stores them in the buffer pointed to by `ptr`.
 *
 * @param ptr Pointer to the buffer where the data will be stored.
 * @param size The size in bytes of each element to be read.
 * @param nmemb The number of elements to read.
 * @param stream The input stream to read from.
 * @return On success, the total number of elements successfully read is returned.
 *         This may be less than `nmemb` if end-of-file or an error is encountered.
 */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!ptr || !stream || size == 0 || nmemb == 0) {
        errno = EINVAL;
        return 0;
    }

    if (!(stream->flags & _IO_READ)) {
        errno = EBADF; // Stream not open for reading
        stream->flags |= _IO_ERROR;
        return 0;
    }

    size_t total_bytes_to_read = size * nmemb;
    size_t bytes_read_so_far = 0;
    unsigned char *current_ptr = (unsigned char *)ptr;

    while (bytes_read_so_far < total_bytes_to_read) {
        // If buffer is empty or depleted, refill it
        if (stream->pos >= stream->end) {
            // If already at EOF, break
            if (stream->flags & _IO_EOF) {
                break;
            }

            ssize_t bytes_from_fd = read(stream->fd, stream->buffer, stream->buf_size);
            // syslog_debug("fread(): read returned %d bytes, buffer is '%s'", bytes_from_fd, stream->buffer);

            if (bytes_from_fd == 0) {
                stream->flags |= _IO_EOF; // Set EOF flag
                break; // End of file
            } else if (bytes_from_fd < 0) {
                stream->flags |= _IO_ERROR; // Set error flag
                // errno is set by the underlying read()
                break; // Read error
            }
            stream->end = bytes_from_fd;
            stream->pos = 0;
        }

        // Calculate how many bytes can be copied from the buffer
        size_t bytes_in_buffer = stream->end - stream->pos;
        size_t bytes_to_copy = total_bytes_to_read - bytes_read_so_far;
        if (bytes_to_copy > bytes_in_buffer) {
            bytes_to_copy = bytes_in_buffer;
        }

        // Copy data from buffer to user's ptr
        memcpy(current_ptr, stream->buffer + stream->pos, bytes_to_copy);

        // Update positions and counts
        stream->pos += bytes_to_copy;
        current_ptr += bytes_to_copy;
        bytes_read_so_far += bytes_to_copy;
    }

    return bytes_read_so_far / size;
}
