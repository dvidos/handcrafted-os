#pragma once

#include <stdlib.h>
#include "dependencies/sector_device.h"
#include "dependencies/mem_allocator.h"


typedef struct simplest_filesystem simplest_filesystem;
typedef struct sfs_handle sfs_handle;

typedef struct sfs_dir_entry {
    const char name[64];
    uint32_t type;
    uint32_t flags;
    uint32_t size;
    uint32_t ctime;
} sfs_dir_entry;

struct simplest_filesystem {
    int (*mkfs)(simplest_filesystem *sfs, char *volume_label, uint32_t desired_block_size);
    int (*fsck)(simplest_filesystem *sfs, int verbose, int repair);
    int (*mount)(simplest_filesystem *sfs, int readonly);
    int (*sync)(simplest_filesystem *sfs);
    int (*unmount)(simplest_filesystem *sfs);
    
    sfs_handle *(*open)(simplest_filesystem *sfs, char *path, int options);
    int (*read)(simplest_filesystem *sfs, sfs_handle *h, uint32_t size, void *buffer);
    int (*write)(simplest_filesystem *sfs, sfs_handle *h, uint32_t size, void *buffer);
    int (*close)(simplest_filesystem *sfs, sfs_handle *h);
    int (*seek)(simplest_filesystem *sfs, sfs_handle *h, int offset, int origin);
    int (*tell)(simplest_filesystem *sfs, sfs_handle *h);

    sfs_handle *(*open_dir)(simplest_filesystem *sfs, char *path);
    int (*read_dir)(simplest_filesystem *sfs, sfs_handle *h, sfs_dir_entry *entry);
    int (*close_dir)(simplest_filesystem *sfs, sfs_handle *h);

    int (*create)(simplest_filesystem *sfs, char *path, int options);
    int (*unlink)(simplest_filesystem *sfs, char *path, int options);
    int (*rename)(simplest_filesystem *sfs, char *oldpath, char *newpath);

    void *sfs_data;
};

simplest_filesystem *new_simplest_filesystem(mem_allocator *memory, sector_device *device);
