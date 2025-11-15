#pragma once

#include "sector_device.h"


typedef struct simplest_filesystem simplest_filesystem;

struct simplest_filesystem {
    int (*mkfs)(simplest_filesystem *sfs, char *volume_label);
    int (*fsck)(simplest_filesystem *sfs, int verbose, int repair);
    int (*mount)(simplest_filesystem *sfs, int readonly);
    int (*sync)(simplest_filesystem *sfs);
    int (*unmount)(simplest_filesystem *sfs);
    
    void *sfs_data;
};


simplest_filesystem *new_simplest_filesystem(sector_device *device);
