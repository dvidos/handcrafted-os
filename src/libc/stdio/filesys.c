#include <ctypes.h>
#include <syscall.h>
#include <stdio.h>

#ifdef __is_libc

dirent_t dirent;


int getcwd(char *buffer, int size) {
    return syscall(SYS_GET_CWD, (int)buffer, size, 0, 0, 0);
}

int setcwd(char *path) {
    return syscall(SYS_SET_CWD, (int)path, 0, 0, 0, 0);
}

int open(char *file) {
    return syscall(SYS_OPEN, (int)file, 0, 0, 0, 0);
}

int read(int handle, char *buffer, int length) {
    return syscall(SYS_READ, handle, (int)buffer, length, 0, 0);
}

int write(int handle, char *buffer, int length) {
    return syscall(SYS_WRITE, handle, (int)buffer, length, 0, 0);
}

int seek(int handle, int offset, enum seek_origin origin) {
    return syscall(SYS_SEEK, handle, offset, (int)origin, 0, 0);
}

int close(int handle) {
    return syscall(SYS_CLOSE, handle, 0, 0, 0, 0);
}

int opendir(char *name) {
    return syscall(SYS_OPEN_DIR, (int)name, 0, 0, 0, 0);
}

int rewinddir(int handle) {
    return syscall(SYS_REWIND_DIR, handle, 0, 0, 0, 0);
}

dirent_t *readdir(int handle) {
    int err = syscall(SYS_READ_DIR, handle, (int)&dirent, 0, 0, 0);
    return err ? NULL : &dirent;
}

int closedir(int handle) {
    return syscall(SYS_CLOSE_DIR, handle, 0, 0, 0, 0);
}

int touch(char *path) {
    return syscall(SYS_TOUCH, (int)path, 0, 0, 0, 0);
}

int mkdir(char *path) {
    return syscall(SYS_MKDIR, (int)path, 0, 0, 0, 0);
}

int unlink(char *path) {
    return syscall(SYS_UNLINK, (int)path, 0, 0, 0, 0);
}


#endif // __is_libc