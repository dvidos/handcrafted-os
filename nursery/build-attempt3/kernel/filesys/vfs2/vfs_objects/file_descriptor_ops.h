#pragma once
#include <ctypes.h>
#include "drivers/fs_driver_ops.h"


struct file_descriptor_ops {
    file_descriptor_t *(*create)(superblock_t *sb, uint64_t inode, file_descriptor_t *dir, const char *name);
    file_descriptor_t *(*clone)(const file_descriptor_t *src);
    int (*equals)(const file_descriptor_t *a, const file_descriptor_t *b);
    void (*destroy)(file_descriptor_t *fd);
    // hashcode? log_debug? get full path?
};

extern struct file_descriptor_ops file_descriptors;
