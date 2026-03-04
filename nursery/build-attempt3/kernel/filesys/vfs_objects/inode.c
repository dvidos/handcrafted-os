#include "inode.h"
#include "../../include/uapi/vfs_file_flags.h"
#include "../memory/kheap.h"
#include "../klib/string.h"
#include "../../utils/logger.h"


MODULE("INODE", LOG_LEVEL_INFO);


static inode_t *_inode_create(superblock_t *sb, uint64_t inode, inode_t *parent, const char *name);
static inode_t *_inode_clone(const inode_t *src);
static bool _inode_equals(const inode_t *a, const inode_t *b);
static void _inode_destroy(inode_t *n);


static inode_t *_inode_create(superblock_t *sb, uint64_t inode, inode_t *parent, const char *name) {
    inode_t *n = (inode_t *)kmalloc(sizeof(inode_t));

    n->sb        = sb;     // which mounted FS
    n->inode_num = inode;  // inode / cluster / object id
    n->mode      = this must be properly initialize, for lookup to work correctly!!!! 0;      // permissions
    n->size      = 0;      // file size in bytes
    n->blocks    = 0;      // allocated blocks
    n->atime     = 0;
    n->mtime     = 0;
    n->ctime     = 0;
    
    n->parent = (parent == NULL) ? NULL : _inode_clone(parent); // path resolution support (optional but useful)

    if (name == NULL) {
        n->name = NULL;
    } else {
        n->name = (char *)kmalloc(strlen(name) + 1);
        strcpy(n->name, name);
    }

    return n;
}

static inode_t *_inode_clone(const inode_t *src) {
    if (src == NULL)
        return NULL;
    
    inode_t *clone = _inode_create(src->sb, src->inode_num, src->parent, src->name);
    clone->mode   = src->mode;
    clone->size   = src->size;
    clone->blocks = src->blocks;
    clone->atime  = src->atime;
    clone->mtime  = src->mtime;
    clone->ctime  = src->ctime;

    return clone;
}

static bool _inode_equals(const inode_t *a, const inode_t *b) {
    if (a == b) return true;
    if (a == NULL && b == NULL) return true; // both are null
    if (a == NULL || b == NULL) return false; // only one is null

    if (a->sb        != b->sb)     return false;
    if (a->inode_num != b->inode_num)  return false;
    if (a->mode      != b->mode)   return false;
    if (a->size      != b->size)   return false;
    if (a->blocks    != b->blocks) return false;
    if (a->atime     != b->atime)  return false;
    if (a->mtime     != b->mtime)  return false;
    if (a->ctime     != b->ctime)  return false;
    
    return true;
}

static void _inode_destroy(inode_t *n) {
    if (n == NULL)
        return;
    
    if (n->parent != NULL)
        _inode_destroy(n->parent);

    if (n->name != NULL)
        kfree(n->name);
    kfree(n);
}

static bool _inode_is_dir(inode_t *n) {
    return (n->mode & S_IFMT) == S_IFDIR;
}

static bool _inode_is_file(inode_t *n) {
    return (n->mode & S_IFMT) == S_IFREG;
}

static void _inode_log_info(const char *var_name, inode_t *n) {
    if (n == NULL) {
        log_info("%s: (null)", var_name);
    } else {
        log_info("%s: sb=%p, inode_no=%llu, mode=%lx, size=%lld, blocks=%lld, atime=%lld, mtime=%lld, ctime=%lld", 
            var_name, 
            n->sb,
            n->inode_num,
            n->mode,
            n->size,
            n->blocks,
            n->atime,
            n->mtime,
            n->ctime
        );
    }
}

struct inode_ops inodes = {
    .create  = _inode_create,
    .clone   = _inode_clone,
    .equals  = _inode_equals,
    .destroy = _inode_destroy,
    .is_dir  = _inode_is_dir,
    .is_file = _inode_is_file,
    .log     = _inode_log_info,
};
