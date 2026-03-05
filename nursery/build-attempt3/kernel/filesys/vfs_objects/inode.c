#include "inode.h"
#include "../../include/uapi/vfs_file_flags.h"
#include "../memory/kheap.h"
#include "../klib/string.h"
#include "../../utils/logger.h"


MODULE("INODE", LOG_LEVEL_INFO);


static inode_t _inode_empty() {
    return (inode_t){
        .sb = NULL,
        .inode_num = 0,
        .mode = 0,
        .size = 0,
        .blocks = 0
    };
}

static inode_t _inode_create(superblock_t *sb, uint64_t inode_no, bool is_dir, bool is_file) {
    inode_t n;

    n.sb        = sb;
    n.inode_num = inode_no;  // inode / cluster / object id
    n.mode      = (is_dir ? 0 : S_IFDIR) + (is_file ? 0 : S_IFREG);
    n.size      = 0;
    n.blocks    = 0;
    n.atime     = 0;
    n.mtime     = 0;
    n.ctime     = 0;

    return n;
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

static bool _inode_is_empty(inode_t *n) {
    return (n->sb == NULL && n->inode_num == 0 && n->mode == 0 && n->size == 0 && n->blocks == 0);
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
        log_info("%s: sb=%p, inode_no=%llu, mode=%lx (%s), size=%lld, blocks=%lld, atime=%lld, mtime=%lld, ctime=%lld", 
            var_name, 
            n->sb,
            n->inode_num,
            n->mode,
            _inode_is_dir(n) ? "dir" : (_inode_is_file(n) ? "reg" : "other"),
            n->size,
            n->blocks,
            n->atime,
            n->mtime,
            n->ctime
        );
    }
}

struct inode_ops inodes = {
    .empty    = _inode_empty,
    .create   = _inode_create,
    .equals   = _inode_equals,
    .is_empty = _inode_is_empty,
    .is_dir   = _inode_is_dir,
    .is_file  = _inode_is_file,
    .log      = _inode_log_info,
};
