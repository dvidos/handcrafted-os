#pragma once

// this file contains the contract that drivers must implement,
// in order to participate in the VFS system

#include "../../misc/lock.h"
#include "../../include/uapi/vfs2_file_flags.h"
#include "../../include/uapi/vfs2_stat.h"

typedef struct superblock      superblock_t;
typedef struct file_descriptor file_descriptor_t;
typedef struct open_file       open_file_t;
typedef struct fs_driver_ops   fs_driver_ops_t;


struct fs_driver_ops {
    int (*mount)(superblock_t *sb);
    int (*unmount)(superblock_t *sb);
    int (*sync)(superblock_t *sb);

    int (*get_root_dir)(superblock_t *sb, file_descriptor_t *out);
    int (*lookup)(file_descriptor_t *dir, const char *name, file_descriptor_t *out);

    int (*open)(file_descriptor_t *fd, int flags, open_file_t *file);
    int (*close)(open_file_t *file);
    int (*read)(open_file_t *file, void *buf, size_t len);
    int (*write)(open_file_t *file, const void *buf, size_t len);
    int (*flush)(open_file_t *file);

    int (*opendir)(file_descriptor_t *dir, open_file_t *dir_handle);
    int (*readdir)(open_file_t *dir_handle, file_descriptor_t *out);
    int (*rewinddir)(open_file_t *dir_handle);
    int (*closedir)(open_file_t *dir_handle);

    int (*create)(file_descriptor_t *parent, const char *name, int type, file_descriptor_t *out);
    int (*unlink)(file_descriptor_t *parent, const char *name);
    int (*mkdir)(file_descriptor_t *parent, const char *name); // dirs have special create semantics
    int (*rmdir)(file_descriptor_t *parent, const char *name); // dirs have special delete semantics

    int (*stat)(file_descriptor_t *fd, struct stat *out);
    int (*truncate)(file_descriptor_t *fd, size_t size);
};


// lives for duration of mount()
struct superblock {       
    fs_driver_ops_t *driver;      // plugin contract
    struct block_device *bdev;    // partition / disk
    void *fs_private_data;        // FS-specific superblock data
    int fs_id;                    // unique mount id (= global_monotonic_counter++)
    lock_t lock;                  // protects fs-level metadata
};


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


// vfs-owned, one per open handle, created/destroyed in vfs_open()/vfs_close()
struct open_file {           
    superblock_t *sb;
    file_descriptor_t *fd;        // immutable identity
    uint64_t offset;              // VFS-owned file position
    uint32_t flags;               // RDONLY, WRONLY, APPEND, etc
    void *fs_private_data;        // driver-specific open context
    lock_t lock;                  // protects offset & state
};

