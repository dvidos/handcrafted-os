#include "file_descriptor.h"
#include "../memory/kheap.h"
#include "../klib/string.h"

static file_descriptor_t *_file_descriptor_create(superblock_t *sb, uint64_t inode, file_descriptor_t *dir, const char *name);
static file_descriptor_t *_file_descriptor_clone(const file_descriptor_t *src);
static bool _file_descriptor_equals(const file_descriptor_t *a, const file_descriptor_t *b);
static void _file_descriptor_destroy(file_descriptor_t *fd);


static file_descriptor_t *_file_descriptor_create(superblock_t *sb, uint64_t inode, file_descriptor_t *dir, const char *name) {
    file_descriptor_t *fd = (file_descriptor_t *)kmalloc(sizeof(file_descriptor_t));

    fd->sb     = sb;     // which mounted FS
    fd->inode  = inode;  // inode / cluster / object id
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
    clone->mode   = src->mode;
    clone->size   = src->size;
    clone->blocks = src->blocks;
    clone->atime  = src->atime;
    clone->mtime  = src->mtime;
    clone->ctime  = src->ctime;

    return clone;
}

static bool _file_descriptor_equals(const file_descriptor_t *a, const file_descriptor_t *b) {
    if (a == b) return true;
    if (a == NULL && b == NULL) return true; // both are null
    if (a == NULL || b == NULL) return false; // only one is null

    if (a->sb     != b->sb)     return false;
    if (a->inode  != b->inode)  return false;
    if (a->mode   != b->mode)   return false;
    if (a->size   != b->size)   return false;
    if (a->blocks != b->blocks) return false;
    if (a->atime  != b->atime)  return false;
    if (a->mtime  != b->mtime)  return false;
    if (a->ctime  != b->ctime)  return false;
    
    return true;
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
