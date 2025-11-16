#pragma once

#include <stdlib.h>
#include "dependencies/sector_device.h"
#include "dependencies/mem_allocator.h"


typedef struct simplest_filesystem simplest_filesystem;
typedef struct sfs_handle sfs_handle;

struct simplest_filesystem {
    int (*mkfs)(simplest_filesystem *sfs, char *volume_label);
    int (*fsck)(simplest_filesystem *sfs, int verbose, int repair);
    int (*mount)(simplest_filesystem *sfs, int readonly);
    int (*sync)(simplest_filesystem *sfs);
    int (*unmount)(simplest_filesystem *sfs);
    
    sfs_handle *(*sfs_open)(simplest_filesystem *sfs, char *filename, int options);
    int (*sfs_read)(simplest_filesystem *sfs, sfs_handle *h, uint32_t size, void *buffer);
    int (*sfs_write)(simplest_filesystem *sfs, sfs_handle *h, uint32_t size, void *buffer);
    int (*sfs_close)(simplest_filesystem *sfs, sfs_handle *h);

    void *sfs_data;
};

simplest_filesystem *new_simplest_filesystem(mem_allocator *memory, sector_device *device);
