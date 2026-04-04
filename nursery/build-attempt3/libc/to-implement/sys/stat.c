#include <sys/stat.h>
#include <stddef.h> // For size_t, NULL
#include <stdbool.h> // For bool

int stat(const char *pathname, struct stat *buf) {
    // TODO: Implement this function
    return 0;
}

int fstat(int fd, struct stat *buf) {
    // TODO: Implement this function
    return 0;
}

int lstat(const char *pathname, struct stat *buf) {
    // TODO: Implement this function
    return 0;
}

int fchown(int fd, uid_t owner, gid_t group) {
    // TODO: Implement this function
    return 0;
}

