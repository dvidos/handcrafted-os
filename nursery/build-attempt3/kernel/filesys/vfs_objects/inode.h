#pragma once
#include "../../include/ctypes.h"
#include "../../include/uapi/vfs_file_flags.h"
#include "superblock.h"


typedef struct inode inode_t;


// value object, copiable, cacheable, can test for equality
struct inode {  
    superblock_t *sb;    // which mounted FS
    uint64_t inode_num;  // inode / file id / synthetic file identifier for FAT
    uint32_t mode;       // file type & permissions, see S_Ixxxx defines
    uint64_t size;       // file size in bytes
    uint64_t blocks;     // allocated blocks
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint16_t perms;
    uid_t    user_id;
    gid_t    group_id;
};


struct inode_ops {
    inode_t (*empty)();
    inode_t (*create)(superblock_t *sb, uint64_t inode, bool is_dir, bool is_file, uint32_t file_size);
    bool (*equals)(const inode_t *a, const inode_t *b);
    bool (*is_empty)(inode_t *n);
    bool (*is_dir)(inode_t *n);
    bool (*is_file)(inode_t *n);
    // hashcode? log_debug? get full path? mutex_acquire()/release()?
    // maybe iget/iput, to track references and destroy when down to zero.`
    void (*log)(const char *var_name, inode_t *n);
};

extern struct inode_ops inodes;
