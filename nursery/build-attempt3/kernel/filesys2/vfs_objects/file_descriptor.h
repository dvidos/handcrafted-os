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


struct file_descriptor_ops {
    file_descriptor_t *(*create)(superblock_t *sb, uint64_t inode, file_descriptor_t *dir, const char *name);
    file_descriptor_t *(*clone)(const file_descriptor_t *src);
    int (*equals)(const file_descriptor_t *a, const file_descriptor_t *b);
    void (*destroy)(file_descriptor_t *fd);
    // hashcode? log_debug? get full path?
};

extern struct file_descriptor_ops file_descriptors;
