#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <stddef.h> // For size_t, NULL
#include <stdbool.h> // For bool

int access(const char *pathname, int mode) {
    // TODO: Implement this function
    return 0;
}

int close(int fd) {
    // TODO: Implement this function
    return 0;
}

int creat(const char *pathname, mode_t mode) {
    // TODO: Implement this function
    return 0;
}

int dup2(int oldfd, int newfd) {
    // TODO: Implement this function
    return 0;
}

int fchown(int fd, uid_t owner, gid_t group) {
    // TODO: Implement this function
    return 0;
}

int fcntl(int fd, int cmd, ...) {
    // TODO: Implement this function
    return 0;
}

int fstat(int fd, struct stat *buf) {
    // TODO: Implement this function
    return 0;
}

int ftruncate(int fd, off_t length) {
    // TODO: Implement this function
    return 0;
}

int link(const char *oldpath, const char *newpath) {
    // TODO: Implement this function
    return 0;
}

off_t lseek(int fd, off_t offset, int whence) {
    // TODO: Implement this function
    return 0;
}

int lstat(const char *pathname, struct stat *buf) {
    // TODO: Implement this function
    return 0;
}

int open(const char *pathname, int flags, ...) {
    // TODO: Implement this function
    return 0;
}

int stat(const char *pathname, struct stat *buf) {
    // TODO: Implement this function
    return 0;
}

int symlink(const char *oldpath, const char *newpath) {
    // TODO: Implement this function
    return 0;
}

int unlink(const char *pathname) {
    // TODO: Implement this function
    return 0;
}

int utime(const char *filename, const struct utimbuf *times) {
    // TODO: Implement this function
    return 0;
}

