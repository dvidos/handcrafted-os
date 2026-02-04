#pragma once
#include "file_descriptor.h"
#include "../memory/kheap.h"
#include "../klib/string.h"

static file_descriptor_t *_file_descriptor_create(superblock_t *sb, uint64_t inode, file_descriptor_t *dir, const char *name);
static file_descriptor_t *_file_descriptor_clone(const file_descriptor_t *src);
static int _file_descriptor_equals(const file_descriptor_t *a, const file_descriptor_t *b);
static void _file_descriptor_destroy(file_descriptor_t *fd);


static file_descriptor_t *_file_descriptor_create(superblock_t *sb, uint64_t inode, file_descriptor_t *dir, const char *name) {
    file_descriptor_t *fd = (file_descriptor_t *)kmalloc(sizeof(file_descriptor_t));

    fd->sb     = sb;     // which mounted FS
    fd->inode  = inode;  // inode / cluster / object id
    fd->type   = 0;      // file, dir, symlink
    fd->mode   = 0;      // permissions
    fd->size   = 0;      // file size in bytes
    fd->blocks = 0;      // allocated blocks
    fd->atime  = 0;
    fd->mtime  = 0;
    fd->ctime  = 0;
    
    fd->parent = (dir == NULL) ? NULL : _file_descriptor_clone(dir); // path resolution support (optional but useful)

    if (name == NULL) {
        fd->name = NULL;
    } else {
        fd->name = (char *)kmalloc(strlen(name) + 1);
        strcpy(fd->name, name);
    }

    return fd;
}

static file_descriptor_t *_file_descriptor_clone(const file_descriptor_t *src) {
    if (src == NULL)
        return NULL;
    
    file_descriptor_t *clone = _file_descriptor_create(src->sb, src->inode, src->parent, src->name);
    clone->type   = src->type;
    clone->mode   = src->mode;
    clone->size   = src->size;
    clone->blocks = src->blocks;
    clone->atime  = src->atime;
    clone->mtime  = src->mtime;
    clone->ctime  = src->ctime;

    return clone;
}

static int _file_descriptor_equals(const file_descriptor_t *a, const file_descriptor_t *b) {
    // compare by value
    if (a == NULL && b == NULL) return 1; // both are null
    if (a == NULL || b == NULL) return 0; // only one is null

    if (a->sb     != b->sb)     return 0;
    if (a->inode  != b->inode)  return 0;
    if (a->type   != b->type)   return 0;
    if (a->mode   != b->mode)   return 0;
    if (a->size   != b->size)   return 0;
    if (a->blocks != b->blocks) return 0;
    if (a->atime  != b->atime)  return 0;
    if (a->mtime  != b->mtime)  return 0;
    if (a->ctime  != b->ctime)  return 0;
    
    return 1;
}

static void _file_descriptor_destroy(file_descriptor_t *fd) {
    if (fd == NULL)
        return;
    
    if (fd->parent != NULL)
        _file_descriptor_destroy(fd->parent);

    if (fd->name != NULL)
        kfree(fd->name);
    kfree(fd);
}

struct file_descriptor_ops file_descriptors = {
    .create  = _file_descriptor_create,
    .clone   = _file_descriptor_clone,
    .equals  = _file_descriptor_equals,
    .destroy = _file_descriptor_destroy,
};
