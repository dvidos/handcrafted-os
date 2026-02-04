#pragma once
#include <ctypes.h>
#include "drivers/vfs_contract.h"


struct open_file_ops {
    open_file_t *(*create)(superblock_t *sb, file_descriptor_t *fd);
    void (*destroy)(open_file_t *f);
    // log_debug?
};

extern struct open_file_ops open_files;
