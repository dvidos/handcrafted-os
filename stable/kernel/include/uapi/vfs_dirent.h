#pragma once
#include "base.h"


// POSIX structure, returned by VFS, exposed to libc and apps
// intentionally minimal
#define MAX_FILE_NAME_LENGTH   128

typedef struct vfs_dirent vfs_dirent_t;
struct vfs_dirent {
    uint32_t       d_ino;    // inode number
    uint32_t       d_type;   // file type (see S_Ixxx flags)
    char           d_name[MAX_FILE_NAME_LENGTH]; // null-terminated filename
};
