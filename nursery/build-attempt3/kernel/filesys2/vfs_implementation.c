#include "vfs_api.h"
#include "../include/uapi/errors.h"
#include "../include/uapi/vfs2_seek_flags.h"
#include "../include/uapi/vfs2_dirent.h"
#include "fs_drivers/fs_driver_ops.h"
#include "vfs_objects/superblock.h"
#include "vfs_objects/inode.h"
#include "vfs_objects/open_file.h"
#include "vfs_objects/mount_table.h"
#include "../klib/path.h"
#include "../klib/string.h"

struct substring {
    const void *ptr;
    unsigned len;
};

static struct substring get_path_first_substring(const char *path) {
    if (path == NULL || *path == 0)
        return (struct substring){.ptr = path, .len = 0};
    
    // skip initial slash
    if (*path == '/')
        path++;
    if (*path == 0)
        return (struct substring){.ptr = path, .len = 0};
    
    // find end slash, if any
    char *slash = strchr(path, '/');
    if (slash == 0)
        return (struct substring){.ptr = path, .len = strlen(path)};
    
    // return just the part, without the final slash
    return (struct substring){.ptr = path, .len = (slash - path)};
}


static int vfs2_flex_lookup(inode_t *start, const char *path, bool lookup_parent, inode_t **fd_out, const char **name_out) {
    if (path == NULL || *path == 0 || path[0] != '/')
        return ERR_BAD_ARGUMENT;
    if (mtab.get_entries_list() == NULL)
        return ERR_NO_FS_MOUNTED;

    inode_t *curr = inodes.clone(start);
    inode_t *next = NULL;
    mount_entry_t *mte;

    char part_buffer[256];
    int part_offset = 0;
    int path_len = strlen(path);
    int err;

    // skip initial root slash, if any
    if (path[part_offset] == '/') {
        curr = mtab.get_entries_list()->root_dir;
        part_offset++;
    }

    while (part_offset < path_len) {

        // see if we are looking for parent dir and we are done
        if (lookup_parent && strchr(path + part_offset, '/') == 0) {
            *fd_out = curr;
            *name_out = path + part_offset;
            return OK;
        }

        // else, we need to continue the path
        struct substring ss = get_path_first_substring(path + part_offset);
        if (ss.len + 1 > sizeof(part_buffer))
            return ERR_NAME_TOO_LONG;
        memcpy(part_buffer, ss.ptr, ss.len);
        part_buffer[ss.len] = 0;
        part_offset += strlen(part_buffer) + 1;

        // skip over empty parts or same dir
        if (strlen(part_buffer) == 0 || strcmp(part_buffer, ".") == 0)
            continue;
        
        if (strcmp(part_buffer, "..") == 0) {
            // we may cross a mount point
            mte = mtab.find_entry_by_root_dir(curr);
            if (mte != NULL && mte->host_dir != NULL) {
                inodes.destroy(curr);
                curr = inodes.clone(mte->host_dir);
            }
            // fallback into leaving the fs driver find the ".." entry
        }

        if (!inodes.is_dir(curr))
            return ERR_NOT_A_DIRECTORY;
        
        err = curr->sb->driver->lookup(curr, part_buffer, &next);
        if (err) return err;

        // we may cross a mount point
        mte = mtab.find_entry_by_host_dir(next);
        if (mte != NULL) {
            inodes.destroy(next);
            next = inodes.clone(mte->root_dir);
        }

        inodes.destroy(curr);
        curr = next;
    }

    *fd_out = curr;
    *name_out = 0;
    return OK;
}

static int vfs2_lookup_target(const char *path, inode_t **target_out) {
    if (path[0] != '/') return ERR_BAD_ARGUMENT; // till we get process cwd

    const char *name_out;
    int err = vfs2_flex_lookup(NULL, path, false, target_out, &name_out);
    if (err) return err;

    return OK;
}

static int vfs2_lookup_parent(const char *path, inode_t **parent_out, const char **final_name_out) {
    if (path[0] != '/') return ERR_BAD_ARGUMENT; // till we get process cwd

    int err = vfs2_flex_lookup(NULL, path, true, parent_out, final_name_out);
    if (err) return err;

    return OK;
}

// ----------------------------------------------------------------------------------------

int vfs2_mount(const char *path, block_device_t *dev, fs_driver_ops_t *driver) {
    int err;
    inode_t *host_dir;

    if (strcmp(path, "/") == 0) {
        // mount without parent
        if (mtab.get_entries_list() != NULL)
            return ERR_DIR_HAS_MOUNT;
        host_dir = NULL;

    } else {
        err = vfs2_lookup_target(path, &host_dir);
        if (err != OK) return err;

        if (!inodes.is_dir(host_dir))
            return ERR_NOT_A_DIRECTORY;

        mount_entry_t *me = mtab.find_entry_by_host_dir(host_dir);
        if (me != NULL) return ERR_DIR_HAS_MOUNT;
    }

    superblock_t *sb = superblocks.create(driver, dev);
    err = driver->mount(sb);
    if (err) return err;

    inode_t *new_root_dir;
    err = driver->get_root_dir(sb, &new_root_dir);
    if (err) return err;

    mount_entry_t *entry = mtab.create_entry(host_dir, new_root_dir);
    err = mtab.add_entry(entry);
    if (err) return err;

    // should release memory if failed
    return OK;
}

int vfs2_unmount(const char *path) {
    int err;
    inode_t *dir;

    err = vfs2_lookup_target(path, &dir);
    if (err) return err;

    mount_entry_t *entry = mtab.find_entry_by_host_dir(dir);
    if (entry == NULL) return ERR_NOT_FOUND;

    err = entry->sb->driver->sync(entry->sb);
    if (err) return err;

    err = entry->sb->driver->unmount(entry->sb);
    if (err) return err;

    err = mtab.remove_entry(entry);
    if (err) return err;
    
    mtab.destroy_entry(entry);
    return OK;
}

int vfs2_sync(void) {
    for (mount_entry_t *entry = mtab.get_entries_list(); entry; entry = entry->next) {
        // ignore errors and try to sync all, anyway
        entry->sb->driver->sync(entry->sb);
    }
    return OK;
}

int vfs2_open(const char *path, int flags, open_file_t **file) {
    int err;
    inode_t *n;

    err = vfs2_lookup_target(path, &n);
    if (err) return err;
    if (!inodes.is_file(n))
        return ERR_NOT_A_FILE;

    err = n->sb->driver->open(n, flags, file);
    if (err) return err;

    // cache size for offset calculations
    (*file)->size = n->size;
    (*file)->offset = 0;

    return OK;
}

int vfs2_close(open_file_t *file) {
    int err = file->sb->driver->close(file);
    if (err) return err;

    open_files.destroy(file);
    return OK;
}

ssize_t vfs2_read(open_file_t *file, void *buf, size_t len) {
    ssize_t bytes = file->sb->driver->read(file, buf, len, file->offset);
    if (bytes < 0) // negative numbers are errors
        return bytes;
    
    // do we need copy-to-user?
    file->offset += bytes;
    return bytes;
}

ssize_t vfs2_write(open_file_t *file, const void *buf, size_t len) {
    ssize_t bytes = file->sb->driver->write(file, buf, len, file->offset);
    if (bytes < 0) // negative numbers are errors
        return bytes;

    // do we need copy-from-user?

    // maintain offset and size of seek
    file->offset += bytes;
    if (file->offset > file->size)
        file->size = file->offset;
    return bytes;
}

off_t vfs2_seek(open_file_t *file, off_t offset, int whence) {
    off_t new_offset;
    switch (whence) {
        case SEEK_SET: new_offset = offset; break;
        case SEEK_CUR: new_offset = file->offset + offset; break;
        case SEEK_END: new_offset = file->size + offset; break;
        default: return ERR_BAD_ARGUMENT;
    }
    if (new_offset < 0) new_offset = 0;
    if (new_offset > (off_t)file->size) new_offset = (off_t)file->size;
    file->offset = new_offset;
    return new_offset;
}

error_t vfs2_flush(open_file_t *file) {
    int err = file->sb->driver->flush(file);
    if (err) return err;

    return OK;
}

error_t vfs2_opendir(const char *path, open_file_t **dir) {
    int err;
    inode_t *n;

    err = vfs2_lookup_target(path, &n);
    if (err) return err;
    if (!inodes.is_dir(n))
        return ERR_NOT_A_DIRECTORY;

    err = n->sb->driver->opendir(n, dir);
    if (err) return err;

    // cache size for offset calculations
    (*dir)->size = n->size;
    (*dir)->offset = 0;

    return OK;
}

ssize_t vfs2_readdir(open_file_t *dir, struct dirent *out) {
    int bytes;

    bytes = dir->sb->driver->readdir(dir, out);
    if (bytes < 0) return bytes; // negative numbers are errors

    dir->offset += bytes;
    return bytes; // returning 0 at end, is more POSIX like
}

error_t vfs2_rewinddir(open_file_t *dir) {
    int err = dir->sb->driver->rewinddir(dir);
    if (err) return err;

    dir->offset = 0;
    return OK;
}

error_t vfs2_closedir(open_file_t *dir) {
    int err = dir->sb->driver->closedir(dir);
    if (err) return err;

    open_files.destroy(dir);
    return OK;
}

error_t vfs2_stat(const char *path, struct stat *out) {
    inode_t *n;
    int err = vfs2_lookup_target(path, &n);
    if (err) return err;

    err = n->sb->driver->stat(n, out);
    if (err) return err;

    return OK;
}

error_t vfs2_fstat(open_file_t *file, struct stat *out) {
    return file->sb->driver->stat(file->n, out);
}

error_t vfs2_truncate(const char *path, size_t size) {
    inode_t *n;
    int err = vfs2_lookup_target(path, &n);
    if (err) return err;

    err = n->sb->driver->truncate(n, size);
    if (err) return err;

    return OK;
}

error_t vfs2_create(const char *path, int type) {
    inode_t *dir;
    const char *name_ptr;
    int err = vfs2_lookup_parent(path, &dir, &name_ptr);
    if (err) return err;

    inode_t *new_file;
    err = dir->sb->driver->create(dir, name_ptr, type, &new_file);
    if (err) return err;

    return OK;
}

error_t vfs2_unlink(const char *path) {
    inode_t *dir;
    const char *name_ptr;
    int err = vfs2_lookup_parent(path, &dir, &name_ptr);
    if (err) return err;

    inode_t *new_file;
    err = dir->sb->driver->unlink(dir, name_ptr);
    if (err) return err;

    return OK;
}

error_t vfs2_mkdir(const char *path) {
    inode_t *dir;
    const char *name_ptr;
    int err = vfs2_lookup_parent(path, &dir, &name_ptr);
    if (err) return err;

    err = dir->sb->driver->mkdir(dir, name_ptr);
    if (err) return err;

    return OK;
}

error_t vfs2_rmdir(const char *path) {
    inode_t *dir;
    const char *name_ptr;
    int err = vfs2_lookup_parent(path, &dir, &name_ptr);
    if (err) return err;

    err = dir->sb->driver->rmdir(dir, name_ptr);
    if (err) return err;

    return OK;
}

