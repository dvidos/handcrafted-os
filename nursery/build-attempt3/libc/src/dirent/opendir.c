#include "../libc_internal.h"

/**
 * @brief Opens a directory stream.
 *
 * This function opens a directory stream corresponding to the directory named by `name`.
 * The stream is positioned at the first entry.
 *
 * @param name The path to the directory to open.
 * @return On success, a pointer to a `DIR` object is returned. On error, NULL is returned,
 *         and `errno` is set appropriately.
 *
 * @implNote
 * A typical implementation would:
 * 1. Open the directory path using a system call (e.g., `open` with `O_DIRECTORY`).
 * 2. Allocate and initialize a `DIR` structure to maintain the state of the directory stream,
 *    including the file descriptor returned by the system call and a buffer for reading entries.
 * 3. Handle potential errors from the system call (e.g., `ENOENT`, `EACCES`, `ENOTDIR`).
 */
DIR *opendir(const char *name) {
    int fd = syscall(SYS_OPEN_DIR, (int)name, 0, 0, 0, 0);
    if (fd < 0) {
        errno = -fd; // Syscalls typically return negative errno on error
        return NULL;
    }

    DIR *dir = (DIR *)malloc(sizeof(DIR));
    if (!dir) {
        close(fd); // Close the opened directory fd
        errno = ENOMEM;
        return NULL;
    }

    dir->fd = fd;
    // The dirent 'entry' member will be populated by readdir

    return dir;
}