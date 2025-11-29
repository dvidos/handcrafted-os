#pragma once

#include <stdlib.h>
#include "dependencies/sector_device.h"
#include "dependencies/mem_allocator.h"


typedef struct simple_filesystem simple_filesystem;
typedef struct sfs_handle sfs_handle;

typedef struct sfs_dir_entry {
    char name[64];
    uint32_t type;
    uint32_t flags;
    uint32_t size;
    uint32_t ctime;
} sfs_dir_entry;

typedef struct sfs_stat_info {
    uint32_t inode_id;
    uint32_t file_size;
    uint32_t type; // file, dir
    uint32_t blocks;
    uint32_t created_at;
    uint32_t modified_at;
} sfs_stat_info;

struct simple_filesystem {
    int (*mkfs)(simple_filesystem *sfs, char *volume_label, uint32_t desired_block_size);
    int (*mount)(simple_filesystem *sfs, int readonly);
    int (*sync)(simple_filesystem *sfs);
    int (*unmount)(simple_filesystem *sfs);
    
    int (*open)(simple_filesystem *sfs, char *path, int options, sfs_handle **handle_ptr);
    int (*read)(simple_filesystem *sfs, sfs_handle *h, void *buffer, uint32_t size);
    int (*write)(simple_filesystem *sfs, sfs_handle *h, void *buffer, uint32_t size);
    int (*close)(simple_filesystem *sfs, sfs_handle *h);
    int (*seek)(simple_filesystem *sfs, sfs_handle *h, int offset, int origin);
    int (*tell)(simple_filesystem *sfs, sfs_handle *h);

    int (*open_dir)(simple_filesystem *sfs, char *path, sfs_handle **handle_ptr);
    int (*read_dir)(simple_filesystem *sfs, sfs_handle *h, sfs_dir_entry *entry);
    int (*close_dir)(simple_filesystem *sfs, sfs_handle *h);

    int (*stat)(simple_filesystem *sfs, char *path, sfs_stat_info *info);
    int (*create)(simple_filesystem *sfs, char *path, int is_dir);
    int (*truncate)(simple_filesystem *sfs, char *path);
    int (*unlink)(simple_filesystem *sfs, char *path, int options);
    int (*rename)(simple_filesystem *sfs, char *oldpath, char *newpath);

    void (*dump_debug_info)(simple_filesystem *sfs, const char *title);
    void *sfs_data;
};

simple_filesystem *new_simple_filesystem(mem_allocator *memory, sector_device *device);
