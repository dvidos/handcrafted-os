#include <stddef.h>

#define O_RDONLY     0x0
#define O_WRONLY     0x1
#define O_RDWR       0x2
#define O_CREAT     0x40
#define O_EXCL      0x80
#define O_APPEND   0x400

// handles already opened: 0=stdin, 1=stdout, 2=stderr

// return file handle, or -1 on error
int open(char *file, int flags) {
    return -1;
}

// return bytes read, -1 on error
size_t read(int handle, void *buffer, size_t count) {
    return -1;
}

// return bytes written, -1 on error
size_t write(int handle, void *buffer, size_t count) {
    return -1;
}

// close file handle, return 0 on success, -1 on error
int close(int handle) {
    return -1;
}

