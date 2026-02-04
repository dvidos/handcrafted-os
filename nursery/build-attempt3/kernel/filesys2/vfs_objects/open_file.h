#pragma once
#include "../../../misc/lock.h"
#include "superblock.h"
#include "file_descriptor.h"


typedef struct open_file open_file_t;


// vfs-owned, one per open handle, created/destroyed in vfs_open()/vfs_close()
struct open_file {           
    superblock_t *sb;
    file_descriptor_t *fd;        // immutable identity
    uint64_t offset;              // VFS-owned file position
    uint32_t flags;               // RDONLY, WRONLY, APPEND, etc
    void *fs_private_data;        // driver-specific open context
    lock_t lock;                  // protects offset & state
};

