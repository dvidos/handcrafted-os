#pragma once
#include <ctypes.h>
#include "../../memory/kheap.h"
#include "inode.h"
#include "superblock.h"

typedef struct mount_entry mount_entry_t;

struct mount_entry {
    // host filesystem
    inode_t *host_dir;

    // mounted filesystem
    superblock_t *sb;
    inode_t *root_dir;

    // options
    uint32_t flags;  // see VFS_MOUNT_*
    int ref_count;

    mount_entry_t *next; // for mtab list
};


struct mount_table_ops {
    mount_entry_t *(*get_entries_list)();
    mount_entry_t *(*create_entry)(inode_t *host_dir, inode_t *new_root_dir);
    void (*destroy_entry)(mount_entry_t *e);
    int (*add_entry)(mount_entry_t *e);
    int (*remove_entry)(mount_entry_t *e);
    mount_entry_t *(*find_entry_by_host_dir)(inode_t *n);
    mount_entry_t *(*find_entry_by_root_dir)(inode_t *n);
};

extern struct mount_table_ops mtab;
