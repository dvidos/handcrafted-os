#pragma once
#include "../../../include/ctypes.h"
#include "../../../klib/string.h"
#include "../../../klib/bitmap.h"
#include "../../../klib/backed_cache.h"
#include "../../../devices/block/block_device.h"
#include "../../../logger/logger.h"
#include "../../../utils/panic.h"
#include "../../../utils/assert.h"

MODULE("SFS", LOG_LEVEL_INFO);

#include "sfs_internal_persistent_structs.h"



#define ROUND_UP(number, step)        ((((number) + (step) - 1) / (step)) * (step))
#define MIN(a, b)                     ((a) < (b) ? (a) : (b))
#define DIV_CEIL(number, divisor)     (((number) + (divisor) - 1) / (divisor))



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

    // we need block bitmap & cache for allocation & fast access
    bitmap_t *block_bitmap;
    backed_cache_t *block_cache;

    // we need inodes bitmap & cache for allocation & fast access
    bitmap_t *inode_bitmap;
    backed_cache_t *inode_cache;

    // generic block sized buffer for I/O
    uint8_t *generic_block_buffer;
};


static inline bool is_name_reserved(const char *name) { return (strcmp(name, ".") == 0 || strcmp(name, "..") == 0); }


// sfs_block.c -- block low level operations
error_t sfs_block_read_from_device(sfs_mount_data *fs_data, uint64_t block_no, void *buffer);
error_t sfs_block_write_to_device(sfs_mount_data *fs_data, uint64_t block_no, const void *buffer);
error_t sfs_block_cache_backend_load(uint64_t key, void *obj_data, void *context);
error_t sfs_block_cache_backend_write(uint64_t key, void *obj_data, void *context);
error_t sfs_cached_read(sfs_mount_data *fs_data, uint64_t block_no, size_t block_offset, void *buffer, size_t buffer_size);
error_t sfs_cached_write(sfs_mount_data *fs_data, uint64_t block_no, size_t block_offset, const void *buffer, size_t buffer_size);
error_t sfs_cached_fill(sfs_mount_data *md, uint64_t block_no, char value);

// sfs_inode2.c -- inode low level operations
error_t sfs_inode2_read_from_device(sfs_mount_data *mt, inode_no_t num, stored_inode *inode);
error_t sfs_inode2_write_to_device(sfs_mount_data *mt, inode_no_t num, stored_inode *inode);
error_t sfs_inode_cache_backend_load(uint64_t key, void *obj_data, void *context);
error_t sfs_inode_cache_backend_write(uint64_t key, void *obj_data, void *context);
error_t sfs_load_inode2(sfs_mount_data *mt, inode_no_t num, stored_inode *inode);
error_t sfs_save_inode2(sfs_mount_data *mt, inode_no_t num, stored_inode *inode);
error_t sfs_allocate_inode2(sfs_mount_data *mt, inode_no_t *new_inode_num);
error_t sfs_release_inode2(sfs_mount_data *mt, inode_no_t num);

// sfs_block_range.c
bool      sfs_block_range_is_empty(block_range range);
uint32_t  sfs_block_range_get_last_block_no(block_range range);
uint32_t  sfs_block_range_get_next_block_no(block_range range);
int       sfs_block_range_get_last_non_empty_index(block_range *arr, int num_items);
bool      sfs_block_range_arr_is_empty(block_range *arr, int num_items);
uint32_t  sfs_block_range_arr_get_last_block_no(block_range *arr, int num_items);
uint32_t  sfs_block_range_arr_get_next_block_no(block_range *arr, int num_items);


// sfs_block_lifecycle.c
error_t sfs_allocate_new_block(sfs_mount_data *md, uint64_t preferred_block, uint64_t *block_no);

// sfs_block_releasing
void sfs_release_block(sfs_mount_data *md, uint64_t block_no);
void sfs_release_block_range(sfs_mount_data *md, block_range range);
void sfs_release_block_range_array(sfs_mount_data *md, block_range *arr, size_t arr_items);

// sfs_block_expanding.c
error_t sfs_expand_range_array(sfs_mount_data *md, block_range *arr, int arr_count, bool *overflow, block_no_t *new_block_no);
error_t sfs_expand_range_block(sfs_mount_data *md, block_no_t indirect_block_no, bool *overflow, block_no_t *new_block_no);
error_t sfs_expand_dbl_indirect_block(sfs_mount_data *md, block_no_t dbl_indirect_block_no, bool *overflow, block_no_t *new_block_no);


// sfs_block_resolving
error_t sfs_node_resolve_data_block(sfs_mount_data *mt, stored_inode *sin, uint32_t block_index, block_no_t *block_no);

// sfs_inode_ops.c
error_t sfs_node_release_all_data_blocks(sfs_mount_data *md, stored_inode *sin);
error_t sfs_node_num_release_all_data_blocks(sfs_mount_data *md, block_no_t inode_num);
error_t sfs_node_expand_data_blocks(sfs_mount_data *md, stored_inode *sin);



// sfs_file_read_write.c (all cached operations)
ssize_t sfs_read_file_data(sfs_mount_data *mt, stored_inode *ind, uint64_t file_pos, void *data, size_t length);
ssize_t sfs_write_file_data(sfs_mount_data *mt, stored_inode *ind, uint64_t file_pos, const void *data, size_t length);
// also:
error_t sfs_load_direntry(sfs_mount_data *mt, stored_inode *dir_inode, size_t entry_no, stored_dir_entry *entry);
error_t sfs_save_direntry(sfs_mount_data *mt, stored_inode *dir_inode, size_t entry_no, stored_dir_entry *entry);
error_t sfs_find_direntry(sfs_mount_data *mt, stored_inode *dir_inode, const char *name, size_t *entry_no, inode_no_t *target_inode_no);
error_t sfs_clear_direntry(sfs_mount_data *mt, stored_inode *dir_inode, size_t entry_no);
error_t sfs_add_direntry(sfs_mount_data *mt, stored_inode *dir_inode, const char *name, inode_no_t target_inode_no, size_t *entry_no);
error_t sfs_check_dir_emptiness(sfs_mount_data *mt, stored_inode *dir_inode, bool *is_empty);


// -------[ checked up to this line ]-------------


// sfs_priv_data.c
error_t sfs_create_fs_mount_data(block_device_t *dev, sfs_mount_data **out);
error_t sfs_sync_fs_mount_data(sfs_mount_data *fs_data);
void sfs_destroy_fs_mount_data(sfs_mount_data *fs_data);



// sfs_debug.c
void sfs_stored_inode_formatter(log_write_stream_t *stream, va_list args);


