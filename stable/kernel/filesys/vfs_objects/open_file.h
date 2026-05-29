#pragma once
#include "../../utils/mutex.h"
#include "superblock.h"
#include "inode.h"
#include "../../logger/logger.h"


// runtime mode operations for kernel
#define FMODE_STREAM   0x0001  // file is a stream, don't track offset/size


typedef struct open_file open_file_t;



// vfs-owned, one per open handle, created/destroyed in vfs_open()/vfs_close()
struct open_file {           
    superblock_t *sb;
    inode_t  inode;         // immutable identity
    uint32_t flags;         // RDONLY, WRONLY, APPEND, etc
    uint32_t fmode;         // VFS-private file mode flags
    uint64_t offset;        // VFS-maintained file position
    uint64_t size;          // VFS-maintained size copy
    uint32_t refcount;      // track file descriptors using this
    void *driver_priv_data; // driver-specific open context
    lock_t lock;            // protects offset & state
};

struct open_file_ops {
    open_file_t *(*create)(superblock_t *sb, inode_t *n, int flags);
    void (*hold)(open_file_t *f);
    void (*release)(open_file_t *f);
    log_formatter_t *formatter;
};

extern struct open_file_ops open_files;
