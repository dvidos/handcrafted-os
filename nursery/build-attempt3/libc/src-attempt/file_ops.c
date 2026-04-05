#include "libc_internal.h"



int access(const char *pathname, int mode) {
    // TODO: Implement this function
    return 0;
}

int creat(const char *pathname, mode_t mode) {
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
    // set the access / modification time for a file
    return 0;
}

int isatty(int fd) {
    // TODO: Implement this function
    return 0;
}

int rename(const char *oldname, const char *newname) {
    // TODO: implement
    return 0;
}



// ------------------------------------------------------------------------------------
// good functions below
// ------------------------------------------------------------------------------------

char *getcwd(char *buffer, size_t size) {
    int err = syscall(SYS_GET_CWD, (int)buffer, size, 0, 0, 0);
    if (err < 0) return NULL;
    return buffer;
}

int chdir(const char *path) {
    return syscall(SYS_CHDIR, (int)path, 0, 0, 0, 0);
}

int open(const char *file, int flags, ...) {
    return syscall(SYS_OPEN, (int)file, flags, 0, 0, 0);
}

ssize_t read(int handle, void *buffer, size_t length) {
    return (ssize_t)syscall(SYS_READ, handle, (int)buffer, length, 0, 0);
}

ssize_t write(int handle, const void *buffer, size_t length) {
    return (ssize_t)syscall(SYS_WRITE, handle, (int)buffer, length, 0, 0);
}

int seek(int handle, int offset, enum seek_origin origin) {
    return syscall(SYS_SEEK, handle, offset, (int)origin, 0, 0);
}

int close(int handle) {
    return syscall(SYS_CLOSE, handle, 0, 0, 0, 0);
}

int touch(char *path) {
    return syscall(SYS_TOUCH, (int)path, 0, 0, 0, 0);
}

int unlink(char *path) {
    return syscall(SYS_UNLINK, (int)path, 0, 0, 0, 0);
}

int dup(int handle) {
    return syscall(SYS_DUP, handle, 0, 0, 0, 0);
}

int dup2(int handle1, int handle2) {
    return syscall(SYS_DUP2, handle1, handle2, 0, 0, 0);
}
