#include "../libc_internal.h"

/**
 * @brief Gets the value of an environment variable.
 *
 * This function searches the environment list for a string of the form
 * "name=value", and returns a pointer to the `value` string.
 *
 * @param name The name of the environment variable to retrieve.
 * @return A pointer to the value of the environment variable, or NULL if
 *         the variable is not found in the environment.
 *
 * @implNote
 * This function accesses the process's environment, which is typically
 * maintained by the operating system or the C runtime. It involves iterating
 * through an array of strings (e.g., `environ` global variable) and parsing them.
 */
// char *getenv(const char *name) {
//     // TODO: Implement getenv for your operating system.
//     // This involves searching the environment list.
//     (void)name; // Suppress unused parameter warning
//     errno = ENOSYS; // Function not implemented
//     return NULL;
// }