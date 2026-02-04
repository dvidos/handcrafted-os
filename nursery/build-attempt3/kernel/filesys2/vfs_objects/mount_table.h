#pragma once
#include <ctypes.h>
#include "file_descriptor.h"
#include "superblock.h"


typedef struct mount_entry {
    // host filesystem
    file_descriptor_t *host_dir;

    // mounted filesystem
    superblock_t *sb;
    file_descriptor_t *root_dir;

    // options
    uint32_t flags;  // see VFS_MOUNT_*
    int ref_count;

} mount_entry_t;


#define MAX_MOUNT_ENTRIES  8
mount_entry_t mount_table[MAX_MOUNT_ENTRIES];


