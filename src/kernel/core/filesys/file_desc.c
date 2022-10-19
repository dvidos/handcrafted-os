#include <filesys/vfs.h>
#include <klib/string.h>
#include <memory/kheap.h>
#include <klog.h>

MODULE("VFS");


file_descriptor_t *create_file_descriptor(struct superblock *superblock, const char *name, uint32_t location) {
    file_descriptor_t *fd = kmalloc(sizeof(file_descriptor_t));

    fd->superblock = superblock;
    fd->name = strdup(name);
    fd->location = location;

    return fd;
}

file_descriptor_t *clone_file_descriptor(const file_descriptor_t *fd) {
    file_descriptor_t *clone = kmalloc(sizeof(file_descriptor_t));

    memcpy(clone, fd, sizeof(file_descriptor_t));
    clone->name = strdup(fd->name);

    return clone;
}

void copy_file_descriptor(file_descriptor_t *dest, const file_descriptor_t *source) {
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
}

bool file_descriptors_equal(const file_descriptor_t *a, const file_descriptor_t *b) {
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


void destroy_file_descriptor(file_descriptor_t *fd) {
    // superblock is referenced, not owned, we don't free() it
    // is it the same with owning directory??? - or else we'd copy it, in clone()
    kfree(fd->name);
    kfree(fd);
}

void debug_file_descriptor(const file_descriptor_t *fd) {
    klog_debug("  superblock: 0x%08x", fd->superblock);
    klog_debug("  name:       \"%s\"", fd->name);
    klog_debug("  location:   %u", fd->location);
    klog_debug("  size:       %u", fd->size);
    klog_debug("  flags:      0x%x (%04bb)", fd->flags, fd->flags);
    klog_debug("  ctime:       %u", fd->ctime);
    klog_debug("  mtime:       %u", fd->mtime);
}
