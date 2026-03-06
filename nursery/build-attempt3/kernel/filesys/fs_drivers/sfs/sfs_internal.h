#pragma once
#include "../../../include/ctypes.h"
#include "../../../klib/string.h"
#include "../../../klib/bitmap.h"
#include "../../../klib/backed_cache.h"
#include "../../../devices/block/block_device.h"

#include "sfs_internal_persistent_structs.h"



#define ROUND_UP(number, step)      ((((number) + (step) - 1) / (step)) * (step))
#define MIN(a, b)                   ((a) < (b) ? (a) : (b))



static inline stored_inode new_stored_inode_file()  { return (stored_inode){ .type_perms = STORED_INODE_TYPE_FILE }; }
static inline stored_inode new_stored_inode_dir()  { return (stored_inode){ .type_perms = STORED_INODE_TYPE_DIR }; }
static inline bool stored_inode_is_dir(stored_inode *n)    { return (n->type_perms & STORED_INODE_TYPE_MASK) == STORED_INODE_TYPE_DIR; }
static inline bool stored_inode_is_file(stored_inode *n)   { return (n->type_perms & STORED_INODE_TYPE_MASK) == STORED_INODE_TYPE_FILE; }
static inline bool stored_inode_is_used(stored_inode *n)   { return (n->type_perms & STORED_INODE_TYPE_MASK) != 0; }
static inline bool stored_inode_is_unused(stored_inode *n) { return (n->type_perms & STORED_INODE_TYPE_MASK) == 0; }



static inline bool stored_dir_entry_is_used(stored_dir_entry *e) { return (e->name[0] != 0); }
static inline bool stored_dir_entry_is_unused(stored_dir_entry *e) { return (e->name[0] == 0); }
static inline void stored_dir_entry_mark_unused(stored_dir_entry *e) { e->name[0] = 0; e->inode_num = 0; }


struct sfs_mount_data {
    block_device_t *dev;

    bool is_readonly;
    stored_superblock *superblock;

    // we need block bitmap and block cache
    bitmap_t *block_bitmap;
    backed_cache_t *block_cache;

    // we need inodes cache for fast resolution
    backed_cache_t *inode_cache;

    // generic block sized buffer for I/O
    uint8_t *generic_block_buffer;

    stored_inode *root_dir;
    stored_inode *inodes_db;
};


static inline bool is_name_reserved(const char *name) { return (strcmp(name, ".") == 0 || strcmp(name, "..") == 0); }


// sfs_block.c
error_t sfs_block_read(sfs_mount_data *fs_data, uint64_t block_no, char *buffer);
error_t sfs_block_write(sfs_mount_data *fs_data, uint64_t block_no, char *buffer);

// sfs_priv_data.c
error_t sfs_create_fs_data(block_device_t *dev, sfs_mount_data **out);
error_t sfs_sync_fs_data(sfs_mount_data *fs_data);
void sfs_destroy_fs_data(sfs_mount_data *fs_data);

// sfs_inode_ops.c
ssize_t sfs_node_read_file_bytes(sfs_mount_data *mt, stored_inode *ind, uint64_t file_pos, void *data, size_t length);
ssize_t sfs_node_write_file_bytes(sfs_mount_data *mt, stored_inode *ind, inode_no_t inode_num, uint64_t file_pos, const void *data, size_t length);
ssize_t sfs_node_read_file_rec(sfs_mount_data *mt, stored_inode *ind, size_t rec_size, uint32_t rec_no, void *rec);
ssize_t sfs_node_write_file_rec(sfs_mount_data *mt, stored_inode *ind, inode_no_t inode_num, size_t rec_size, uint32_t rec_no, const void *rec);
// ---
error_t sfs_node_dir_find_entry(sfs_mount_data *mt, stored_inode *dir_node, const char *name, uint32_t *rec_no, inode_no_t *inode_no);
error_t sfs_node_dir_set_entry(sfs_mount_data *mt, inode_no_t dir_node_no, stored_inode *dir_node, uint32_t rec_no, const char *name, inode_no_t inode_no);
error_t sfs_node_dir_add_entry(sfs_mount_data *mt, inode_no_t dir_node_no, stored_inode *dir_node, const char *name, inode_no_t inode_no);
error_t sfs_node_dir_is_empty(sfs_mount_data *mt, stored_inode *sin, bool ignore_special_dirs, bool *is_empty);
// ---
error_t sfs_inodes_db_append(sfs_mount_data *mt, stored_inode *sin, inode_no_t *inode_no);


// sfs_data_blocks.c
error_t sfs_node_resolve_data_block(sfs_mount_data *mt, stored_inode *sin, uint32_t block_index, block_no_t *block_no);
error_t sfs_node_expand_data_blocks(sfs_mount_data *md, stored_inode *sin, inode_no_t inode_num);
error_t sfs_node_release_all_data_blocks(sfs_mount_data *md, block_no_t inode_num);



// sfs_debug.c
void sfs_stored_inode_log_debug(const char *prefix, stored_inode *sin);



