#pragma once
#include "ints.h"

// POSIX structure, returned by VFS, exposed to libc and apps
typedef struct vfs_stat vfs_stat_t;

struct vfs_stat {
    uint64_t st_dev;     // filesystem id (sb.fs_id)
    uint64_t st_ino;     // inode number
    uint32_t st_mode;    // file type + permissions
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_size;    // file size in bytes
    uint64_t st_blocks;  // number of blocks
    uint32_t st_blksize; // block size
    uint64_t st_atime1;  // there are "st_atime" and other macros in sys/stat.h
    uint64_t st_mtime1;   // 
    uint64_t st_ctime1;
};
