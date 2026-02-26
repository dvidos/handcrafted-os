#pragma once
#include "../../include/ctypes.h"
#include "superblock.h"


typedef struct inode inode_t;


// value object, copiable, cacheable, can test for equality
struct inode {  
    superblock_t *sb;             // which mounted FS
    uint64_t inode_num;           // inode / file id / synthetic file identifier for FAT
    uint32_t mode;                // file type & permissions, see S_Ixxxx defines
    uint64_t size;                // file size in bytes
    uint64_t blocks;              // allocated blocks
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    // path resolution support (optional but useful)
    struct inode *parent;  // owned copy or NULL
    char *name;                      // owned
};


struct inode_ops {
    inode_t *(*create)(superblock_t *sb, uint64_t inode, inode_t *parent, const char *name);
    inode_t *(*clone)(const inode_t *src);
    bool (*equals)(const inode_t *a, const inode_t *b);
    void (*destroy)(inode_t *n);
    bool (*is_dir)(inode_t *n);
    bool (*is_file)(inode_t *n);
    // hashcode? log_debug? get full path? mutex_acquire()/release()?
    // maybe iget/iput, to track references and destroy when down to zero.`
};

extern struct inode_ops inodes;
