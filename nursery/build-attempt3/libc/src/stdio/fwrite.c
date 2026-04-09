#include "../libc_internal.h"

/**
 * @brief Writes data from a buffer to a stream.
 *
 * This function writes `nmemb` elements of `size` bytes each from the buffer
 * pointed to by `ptr` to the output `stream`.
 *
 * @param ptr Pointer to the buffer containing the data to write.
 * @param size The size in bytes of each element to be written.
 * @param nmemb The number of elements to write.
 * @param stream The output stream to write to.
 * @return On success, the total number of elements successfully written is returned.
 *         This may be less than `nmemb` if an error is encountered.
 */
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!ptr || !stream || size == 0 || nmemb == 0) {
        errno = EINVAL;
        return 0;
    }

    if (!(stream->flags & _IO_WRITE)) {
        errno = EBADF; // Stream not open for writing
        stream->flags |= _IO_ERROR;
        return 0;
    }

    size_t total_bytes_to_write = size * nmemb;
    size_t bytes_written_so_far = 0;
    const unsigned char *current_ptr = (const unsigned char *)ptr;

    while (bytes_written_so_far < total_bytes_to_write) {
        // Calculate space available in the buffer
        size_t space_in_buffer = stream->buf_size - stream->pos;
        size_t bytes_to_copy_to_buffer = total_bytes_to_write - bytes_written_so_far;

        if (bytes_to_copy_to_buffer > space_in_buffer) {
            bytes_to_copy_to_buffer = space_in_buffer;
        }

        // Copy data to the buffer
        memcpy(stream->buffer + stream->pos, current_ptr, bytes_to_copy_to_buffer);
        stream->pos += bytes_to_copy_to_buffer;
        current_ptr += bytes_to_copy_to_buffer;
        bytes_written_so_far += bytes_to_copy_to_buffer;

        // If buffer is full, flush it
        if (stream->pos == stream->buf_size) {
            ssize_t written_to_fd = write(stream->fd, stream->buffer, stream->pos);
            if (written_to_fd != stream->pos) {
                stream->flags |= _IO_ERROR; // Set error flag
                // errno is set by underlying write()
                return bytes_written_so_far / size; // Return partially written count
            }
            stream->pos = 0; // Reset buffer position after flushing
        }
    }

    return bytes_written_so_far / size;
}
