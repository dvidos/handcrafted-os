#pragma once
#include <ctypes.h>
#include "../../memory/kheap.h"
#include "file_descriptor.h"
#include "superblock.h"

typedef struct mount_entry mount_entry_t;

struct mount_entry {
    // host filesystem
    file_descriptor_t *host_dir;

    // mounted filesystem
    superblock_t *sb;
    file_descriptor_t *root_dir;

    // options
    uint32_t flags;  // see VFS_MOUNT_*
    int ref_count;

    mount_entry_t *next;
};


extern mount_entry_t *mtab_entries_list_head;  // THE filesystem variable


mount_entry_t *create_mount_entry(file_descriptor_t *host_dir, file_descriptor_t *new_root_dir);
void destroy_mount_entry(mount_entry_t *e);
void mtab_mount(mount_entry_t *e);
void mtab_unmount(mount_entry_t *e);
mount_entry_t *mtab_find_by_host_dir(file_descriptor_t *fd);
mount_entry_t *mtab_find_by_root_dir(file_descriptor_t *fd);

