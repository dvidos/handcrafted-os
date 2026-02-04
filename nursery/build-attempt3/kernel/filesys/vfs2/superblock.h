#pragma once
#include <ctypes.h>
#include "drivers/vfs_contract.h"


struct superblock_ops {
    superblock_t *(*create)(superblock_t *sb, file_descriptor_t *fd);
    void (*destroy)(superblock_t *sb);
    // log_debug?
};

extern struct superblock_ops superblocks;
