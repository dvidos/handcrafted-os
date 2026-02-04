#pragma once
#include <ctypes.h>
#include "superblock.h"


typedef struct file_descriptor file_descriptor_t;


// value object, copiable, cacheable, can test for equality
struct file_descriptor {  
    superblock_t *sb;             // which mounted FS
    uint64_t inode;               // inode / cluster / object id
    uint32_t type;                // file, dir, symlink
    uint32_t mode;                // permissions
    uint64_t size;                // file size in bytes
    uint64_t blocks;              // allocated blocks
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    // path resolution support (optional but useful)
    struct file_descriptor *parent;  // owned copy or NULL
    char *name;                      // owned
};
