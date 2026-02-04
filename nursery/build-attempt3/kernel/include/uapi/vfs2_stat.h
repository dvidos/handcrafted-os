#pragma once
#include <ctypes.h>


// POSIX structure, returned by VFS, exposed to libc and apps
struct stat {
    uint64_t st_dev;     // filesystem id (sb.fs_id)
    uint64_t st_ino;     // inode number
    uint32_t st_mode;    // file type + permissions
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_size;    // file size in bytes
    uint64_t st_blocks;  // number of blocks
    uint32_t st_blksize; // block size
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
};
