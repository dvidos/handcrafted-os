#include "../libc_internal.h"

/**
 * @brief Resets the file position indicator to the beginning of the file.
 *
 * This function sets the file position indicator for the `stream` to the
 * beginning of the file. It also clears the end-of-file and error indicators
 * for the stream.
 *
 * @param stream The `FILE` stream.
 */
void rewind(FILE *stream) {
    if (!stream) {
        // Standard says behavior is undefined for NULL stream, but we'll handle gracefully
        return;
    }
    fseek(stream, 0L, SEEK_SET);
    clearerr(stream);
}