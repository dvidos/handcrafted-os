#include "../libc_internal.h"

/**
 * @brief Adds or changes an environment variable.
 *
 * This function adds the variable `name` to the environment with value `value`,
 * or changes the value of `name` if it already exists. If `overwrite` is non-zero,
 * an existing variable is overwritten. If `overwrite` is zero, an existing variable
 * is not changed.
 *
 * @param name The name of the environment variable.
 * @param value The value to set for the environment variable.
 * @param overwrite If non-zero, overwrite existing variable; if zero, do not overwrite.
 * @return 0 on success, or -1 on error with `errno` set.
 *
 * @implNote
 * This function modifies the process's environment. It typically involves:
 * 1. Searching for `name` in the existing environment.
 * 2. If found and `overwrite` is true, freeing the old value and replacing it.
 * 3. If not found, adding a new "name=value" string to the environment.
 * 4. This often requires dynamic memory allocation and management of the environment array.
 */
int setenv(const char *name, const char *value, int overwrite) {
    // TODO: Implement setenv for your operating system.
    // This involves modifying the environment list with specific overwrite logic.
    (void)name;      // Suppress unused parameter warning
    (void)value;     // Suppress unused parameter warning
    (void)overwrite; // Suppress unused parameter warning
    errno = ENOSYS;  // Function not implemented
    return -1;
}