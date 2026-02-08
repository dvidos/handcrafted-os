#include "vfs.h"
#include "../klib/string.h"
#include "../memory/kheap.h"
#include "../utils/logger.h"

MODULE("VFS", LOG_LEVEL_WARN);


open_file_t *create_file_t(superblock_t *superblock, inode_t *inode) {
    open_file_t *file = kmalloc(sizeof(open_file_t));

    file->superblock = superblock; // referenced, not freed
    file->inode = inode; // referenced, not freed

    return file;
}


void debug_file_t(open_file_t *file) {
    log_debug("  file->superblock : 0x%x", file->superblock);
    log_debug("  file->inode      : 0x%x", file->inode);
    log_debug("  file->fs_driver_private_data: 0x%x", file->fs_driver_private_data);
}

void destroy_file_t(open_file_t *file) {
    // superblock is referenced, not owned, we don't free() it
    // same with inode.

    kfree(file);
}

