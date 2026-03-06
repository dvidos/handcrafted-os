#pragma once
#include "../../utils/mutex.h"
#include "superblock.h"
#include "inode.h"
#include "../../logger/logger.h"


typedef struct open_file open_file_t;


// vfs-owned, one per open handle, created/destroyed in vfs_open()/vfs_close()
struct open_file {           
    superblock_t *sb;
    inode_t  inode;         // immutable identity
    uint64_t offset;        // VFS-maintained file position
    uint64_t size;          // VFS-maintained size copy
    uint32_t flags;         // RDONLY, WRONLY, APPEND, etc
    void *driver_priv_data; // driver-specific open context
    lock_t lock;            // protects offset & state
};


struct open_file_ops {
    open_file_t *(*create)(superblock_t *sb, inode_t *n);
    void (*destroy)(open_file_t *f);
    void (*log)(log_level_t level, const char *preamble, open_file_t *f);
};

extern struct open_file_ops open_files;
