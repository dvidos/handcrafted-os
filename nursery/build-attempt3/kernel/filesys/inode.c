#include "vfs.h"
#include "../klib/string.h"
#include "../memory/kheap.h"
#include "../utils/logger.h"

MODULE("VFS", LOG_LEVEL_WARN);


inode_t *create_inode(superblock_t *superblock, const char *name, uint32_t location, inode_t *owning_dir) {
    inode_t *n = kmalloc(sizeof(inode_t));
    memset(n, 0, sizeof(inode_t));

    n->superblock = superblock;
    n->name = strdup(name);
    n->location = location;
    n->owning_directory = clone_inode(owning_dir);

    return n;
}

inode_t *clone_inode(const inode_t *n) {
    if (n == NULL)
        return NULL;
    inode_t *clone = kmalloc(sizeof(inode_t));
    memcpy(clone, n, sizeof(inode_t));
    clone->name = strdup(n->name);
    
    if (n->owning_directory != NULL)
        clone->owning_directory = clone_inode(n->owning_directory);

    return clone;
}

void copy_inode(inode_t *dest, const inode_t *source) {
    // we don't own superblock, so we copy the pointer.
    dest->superblock = source->superblock;

    dest->location = source->location;
    dest->size = source->size;
    dest->flags = source->flags;
    dest->ctime = source->ctime;
    dest->mtime = source->mtime;

    // we own name, we copy the contents of it as well.
    if (dest->name != NULL)
        kfree(dest->name);
    dest->name = strdup(source->name);

    if (dest->owning_directory != NULL)
        destroy_inode(dest->owning_directory);
    dest->owning_directory = clone_inode(source->owning_directory);
}

bool inodes_equal(const inode_t *a, const inode_t *b) {
    if ((a == NULL && b == NULL) || (a == b))
        return true;
    
    // see if they point to the same file.
    // they need to refer to same filesystem and same file location
    if (a->superblock == NULL ||
        a->superblock->partition == NULL ||
        a->superblock->partition->dev == NULL ||
        b->superblock == NULL ||
        b->superblock->partition == NULL ||
        b->superblock->partition->dev == NULL)
        return false;
    
    return (
        a->superblock->partition->dev->dev_no == b->superblock->partition->dev->dev_no &&
        a->superblock->partition->part_no == b->superblock->partition->part_no &&
        a->location == b->location
    );
}


void destroy_inode(inode_t *n) {
    // superblock is referenced, not owned, we don't free() it
    if (n->owning_directory != NULL)
        destroy_inode(n->owning_directory);
    kfree(n->name);
    kfree(n);
}

void debug_inode(const inode_t *n, int depth) {
    char indent[36 + 1];
    memset(indent, ' ', sizeof(indent));
    indent[sizeof(indent) - 1] = 0;
    if (depth <= (int)((sizeof(indent) - 1) / 3))
        indent[depth * 3] = 0;
    
    log_debug("%s  inode at 0x%x, name: \"%s\"", indent, n, n->name);
    log_debug("%s  flags: 0x%02x / %08bb (file=%d, dir=%d)", indent, n->flags, n->flags, FD_FILE, FD_DIR);
    log_debug("%s  location: %u, size: %u, ctime: %u, mtime: %u", indent, n->location, n->size, n->ctime, n->mtime);
    log_debug("%s  superblock: 0x%x,  parent: 0x%x", indent, n->superblock, n->owning_directory);

    if (n->owning_directory != NULL)
        debug_inode(n->owning_directory, depth + 1);
}

int inode_get_full_path_length(const inode_t *n) {
    if (n == NULL || n->name == NULL)
        return 0;
    
    int len = strlen(n->name);
    if (n->owning_directory != NULL)
        // allow room for path separator
        len += 1 + inode_get_full_path_length(n->owning_directory);

    return len;
}


static void _recursively_copy_full_path(const inode_t *n, char *full_path) {
    
    // first copy parent (top will be "/")
    if (n->owning_directory != NULL) {
        _recursively_copy_full_path(n->owning_directory, full_path);

        // add separator, unless it was the root dir
        if (strcmp(n->owning_directory->name, "/") != 0)
            strcat(full_path, "/");
    }

    // then copy our name
    strcat(full_path, n->name);
}

// caller to free path
void inode_get_full_path(const inode_t *n, char **full_path) {
    if (n == NULL || n->name == NULL) {
        (*full_path) = NULL;
        return;
    }

    int len = inode_get_full_path_length(n);
    (*full_path) = kmalloc(len + 1);
    _recursively_copy_full_path(n, *full_path);
}
