#include <filesys/vfs.h>
#include <klib/string.h>
#include <memory/kheap.h>
#include <klog.h>

MODULE("VFS");


file_t *create_file_t(superblock_t *superblock, file_descriptor_t *descriptor) {
    file_t *file = kmalloc(sizeof(file_t));

    file->superblock = superblock; // referenced, not freed
    file->descriptor = descriptor; // referenced, not freed

    return file;
}


void debug_file_t(file_t *file) {
    klog_debug("  file->superblock : 0x%x", file->superblock);
    klog_debug("  file->desciptor : 0x%x", file->descriptor);
    klog_debug("  file->fs_driver_private_data: 0x%x", file->fs_driver_private_data);
}

void destroy_file_t(file_t *file) {
    // superblock is referenced, not owned, we don't free() it
    // same with file descriptor.

    kfree(file);
}

