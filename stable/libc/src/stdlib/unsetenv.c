#include "../libc_internal.h"

/**
 * @brief Deletes an environment variable.
 *
 * This function deletes the variable `name` from the environment.
 *
 * @param name The name of the environment variable to delete.
 * @return 0 on success, or -1 on error with `errno` set.
 *
 * @implNote
 * This function modifies the process's environment. It typically involves:
 * 1. Searching for `name` in the existing environment.
 * 2. If found, removing the "name=value" string from the environment array,
 *    and potentially compacting the array.
 * This often requires dynamic memory management of the environment array.
 */
// int unsetenv(const char *name) {
//     // TODO: Implement unsetenv for your operating system.
//     // This involves removing an environment variable from the list.
//     (void)name; // Suppress unused parameter warning
//     errno = ENOSYS; // Function not implemented
//     return -1;
// }