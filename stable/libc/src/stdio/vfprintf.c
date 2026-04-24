#include "../libc_internal.h"

/**
 * @brief Prints formatted output to a specified stream using a `va_list` argument.
 *
 * This function is equivalent to `fprintf` but accepts a `va_list` object
 * that contains the variable arguments instead of taking them directly.
 *
 * @param stream The output stream to write to.
 * @param format The format string.
 * @param ap A `va_list` object initialized by `va_start`.
 * @return On success, the total number of characters written is returned.
 *         On error, a negative value is returned.
 */
int vfprintf(FILE *stream, const char *format, va_list ap) {
    if (!stream || !format) {
        errno = EINVAL;
        return -1;
    }

    if (!(stream->flags & _IO_WRITE)) {
        errno = EBADF; // Stream not open for writing
        stream->flags |= _IO_ERROR;
        return -1;
    }

    // Try with a small stack-allocated buffer first
    char static_buf[256];
    va_list ap_copy;
    va_copy(ap_copy, ap); // Copy va_list as vsnprintf might consume it

    int len = vsnprintf(static_buf, sizeof(static_buf), format, ap_copy);
    va_end(ap_copy);

    if (len < 0) {
        // Encoding error or other error in vsnprintf
        stream->flags |= _IO_ERROR;
        return -1;
    }

    if ((size_t)len < sizeof(static_buf)) {
        // Static buffer was large enough
        return fwrite(static_buf, 1, len, stream);
    } else {
        // Static buffer was too small, allocate dynamically
        char *dynamic_buf = (char *)malloc(len + 1);
        if (!dynamic_buf) {
            errno = ENOMEM;
            stream->flags |= _IO_ERROR;
            return -1;
        }

        va_copy(ap_copy, ap); // Need to copy again for the second vsnprintf call
        vsnprintf(dynamic_buf, len + 1, format, ap_copy);
        va_end(ap_copy);

        int written_len = fwrite(dynamic_buf, 1, len, stream);
        free(dynamic_buf);
        return written_len;
    }
}