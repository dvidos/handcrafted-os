#pragma once
#include <ctypes.h>


// POSIX structure, returned by VFS, exposed to libc and apps
// intentionally minimal
struct dirent {
    uint32_t       d_ino;    // inode number
    char           d_name[]; // null-terminated filename
};
