#include "../libc_internal.h"

/**
 * @brief Creates a new file or truncates an existing one.
 *
 * This function is equivalent to `open(pathname, O_WRONLY | O_CREAT | O_TRUNC, mode)`.
 * It creates a new file if `pathname` does not exist, or truncates it to
 * zero length if it does exist. The file is opened for write-only access.
 *
 * @param pathname The path to the file to create or truncate.
 * @param mode The file permissions for the newly created file.
 * @return On success, a new file descriptor is returned. On error, -1 is returned,
 *         and `errno` is set.
 */
int creat(const char *pathname, mode_t mode) {
    return open(pathname, O_WRONLY | O_CREAT | O_TRUNC, mode);
}
