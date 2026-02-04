#pragma once
#include "open_file.h"
#include "../memory/kheap.h"
#include "../klib/string.h"


static open_file_t *_open_file_create(superblock_t *sb, file_descriptor_t *fd) {
    open_file_t *f = (open_file_t *)kmalloc(sizeof(open_file_t));

    f->sb = sb;             // superblock (operations)
    f->fd = fd;             // immutable identity
    f->offset = 0;          // VFS-owned file position
    f->flags = 0;           // RDONLY, WRONLY, APPEND, etc
    f->fs_private_data = 0; // driver-specific open context
    f->lock = 0;            // protects offset & state

    return f;
}

static void _open_file_destroy(open_file_t *f) {
    if (f == NULL)
        return;
    kfree(f);
}

struct open_file_ops open_files = {
    .create  = _open_file_create,
    .destroy = _open_file_destroy,
};
