#pragma once
#include "../../utils/mutex.h"
#include "superblock.h"
#include "file_descriptor.h"


typedef struct open_file open_file_t;


// vfs-owned, one per open handle, created/destroyed in vfs_open()/vfs_close()
struct open_file {           
    superblock_t *sb;
    file_descriptor_t *fd;        // immutable identity
    uint64_t offset;              // VFS-maintained file position
    uint64_t size;                // VFS-maintained size copy
    uint32_t flags;               // RDONLY, WRONLY, APPEND, etc
    void *driver_priv_data;       // driver-specific open context
    lock_t lock;                  // protects offset & state
};


struct open_file_ops {
    open_file_t *(*create)(superblock_t *sb, file_descriptor_t *fd);
    void (*destroy)(open_file_t *f);
    // log_debug?
};

extern struct open_file_ops open_files;
