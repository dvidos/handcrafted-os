#include <filesys/vfs.h>
#include <klib/string.h>
#include <memory/kheap.h>
#include <klog.h>

MODULE("VFS");


file_descriptor_t *create_file_descriptor(superblock_t *superblock, const char *name, uint32_t location, file_descriptor_t *owning_dir) {
    file_descriptor_t *fd = kmalloc(sizeof(file_descriptor_t));
    memset(fd, 0, sizeof(file_descriptor_t));

    fd->superblock = superblock;
    fd->name = strdup(name);
    fd->location = location;
    fd->owning_directory = clone_file_descriptor(owning_dir);

    return fd;
}

file_descriptor_t *clone_file_descriptor(const file_descriptor_t *fd) {
    if (fd == NULL)
        return NULL;

    file_descriptor_t *clone = kmalloc(sizeof(file_descriptor_t));
    memcpy(clone, fd, sizeof(file_descriptor_t));
    clone->name = strdup(fd->name);
    
    if (fd->owning_directory != NULL)
        clone->owning_directory = clone_file_descriptor(fd->owning_directory);

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

    if (dest->owning_directory != NULL)
        destroy_file_descriptor(dest->owning_directory);
    dest->owning_directory = clone_file_descriptor(source->owning_directory);
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
    if (fd->owning_directory != NULL)
        destroy_file_descriptor(fd->owning_directory);
    kfree(fd->name);
    kfree(fd);
}

void debug_file_descriptor(const file_descriptor_t *fd, int depth) {
    char indent[36 + 1];
    memset(indent, ' ', sizeof(indent));
    indent[sizeof(indent) - 1] = 0;
    if (depth <= (int)((sizeof(indent) - 1) / 3))
        indent[depth * 3] = 0;
    
    klog_debug("%s  file descriptor at 0x%x, name: \"%s\"", indent, fd, fd->name);
    klog_debug("%s  flags: 0x%02x / %08bb (file=%d, dir=%d)", indent, fd->flags, fd->flags, FD_FILE, FD_DIR);
    klog_debug("%s  location: %u, size: %u, ctime: %u, mtime: %u", indent, fd->location, fd->size, fd->ctime, fd->mtime);
    klog_debug("%s  superblock: 0x%x,  parent: 0x%x", indent, fd->superblock, fd->owning_directory);

    if (fd->owning_directory != NULL)
        debug_file_descriptor(fd->owning_directory, depth + 1);
}

int file_descriptor_get_full_path_length(const file_descriptor_t *fd) {
    if (fd == NULL || fd->name == NULL)
        return 0;
    
    int len = strlen(fd->name);
    if (fd->owning_directory != NULL)
        // allow room for path separator
        len += 1 + file_descriptor_get_full_path_length(fd->owning_directory);

    return len;
}


static void _recursively_copy_full_path(const file_descriptor_t *fd, char *full_path) {
    
    // first copy parent (top will be "/")
    if (fd->owning_directory != NULL) {
        _recursively_copy_full_path(fd->owning_directory, full_path);

        // add separator, unless it was the root dir
        if (strcmp(fd->owning_directory->name, "/") != 0)
            strcat(full_path, "/");
    }

    // then copy our name
    strcat(full_path, fd->name);
}

// caller to free path
void file_descriptor_get_full_path(const file_descriptor_t *fd, char **full_path) {
    if (fd == NULL || fd->name == NULL) {
        (*full_path) = NULL;
        return;
    }

    int len = file_descriptor_get_full_path_length(fd);
    (*full_path) = kmalloc(len + 1);
    _recursively_copy_full_path(fd, *full_path);
}
