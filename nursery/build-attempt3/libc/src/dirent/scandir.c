#include "libc_internal.h"

/**
 * @brief Scans a directory for entries and optionally sorts them.
 *
 * This function reads the directory `dirp` and builds an array of pointers
 * to directory entries that match the `filter` function. The array can then
 * be sorted using the `compar` function.
 *
 * @param dirp The path to the directory to scan.
 * @param namelist A pointer to a pointer to a `struct dirent` array. On success,
 *                 this will be allocated and filled with pointers to matching entries.
 * @param filter A pointer to a function that selects directory entries. If NULL,
 *               all entries are selected.
 * @param compar A pointer to a function that compares two directory entries for sorting.
 *               If NULL, the `namelist` array is not sorted.
 * @return On success, the number of entries in the `namelist` array is returned.
 *         On error, -1 is returned, and `errno` is set.
 *
 * @implNote
 * A typical implementation would:
 * 1. Open and read the directory using `opendir` and `readdir`.
 * 2. For each entry, apply the `filter` function. If it matches, allocate memory
 *    for a new `struct dirent` (or copy the current one) and store its pointer.
 * 3. Dynamically grow the `namelist` array as needed (e.g., using `realloc`).
 * 4. If `compar` is not NULL, sort the `namelist` array using `qsort`.
 * 5. On failure, free any allocated memory before returning.
 */
int scandir(const char *dirp, struct dirent ***namelist,
            int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **, const struct dirent **)) {
    // TODO: Implement scandir for your operating system.
    (void)dirp;     // Suppress unused parameter warning
    (void)namelist; // Suppress unused parameter warning
    (void)filter;   // Suppress unused parameter warning
    (void)compar;   // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return -1;
}