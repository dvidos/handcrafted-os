#pragma once

#include <stdlib.h>
#include "dependencies/sector_device.h"
#include "dependencies/mem_allocator.h"


typedef struct simple_filesystem simple_filesystem;
typedef struct sfs_handle sfs_handle;

typedef struct sfs_dir_entry {
    const char name[64];
    uint32_t type;
    uint32_t flags;
    uint32_t size;
    uint32_t ctime;
} sfs_dir_entry;

struct simple_filesystem {
    int (*mkfs)(simple_filesystem *sfs, char *volume_label, uint32_t desired_block_size);
    int (*fsck)(simple_filesystem *sfs, int verbose, int repair);
    int (*mount)(simple_filesystem *sfs, int readonly);
    int (*sync)(simple_filesystem *sfs);
    int (*unmount)(simple_filesystem *sfs);
    
    sfs_handle *(*open)(simple_filesystem *sfs, char *path, int options);
    int (*read)(simple_filesystem *sfs, sfs_handle *h, uint32_t size, void *buffer);
    int (*write)(simple_filesystem *sfs, sfs_handle *h, uint32_t size, void *buffer);
    int (*close)(simple_filesystem *sfs, sfs_handle *h);
    int (*seek)(simple_filesystem *sfs, sfs_handle *h, int offset, int origin);
    int (*tell)(simple_filesystem *sfs, sfs_handle *h);

    sfs_handle *(*open_dir)(simple_filesystem *sfs, char *path);
    int (*read_dir)(simple_filesystem *sfs, sfs_handle *h, sfs_dir_entry *entry);
    int (*close_dir)(simple_filesystem *sfs, sfs_handle *h);

    int (*create)(simple_filesystem *sfs, char *path, int options);
    int (*unlink)(simple_filesystem *sfs, char *path, int options);
    int (*rename)(simple_filesystem *sfs, char *oldpath, char *newpath);

    void *sfs_data;
};

simple_filesystem *new_simple_filesystem(mem_allocator *memory, sector_device *device);
