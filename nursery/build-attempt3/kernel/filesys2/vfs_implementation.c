#include "vfs_api.h"
#include "../include/uapi/errors.h"
#include "../include/uapi/vfs2_seek_flags.h"
#include "../include/uapi/vfs2_dirent.h"
#include "fs_drivers/fs_driver_ops.h"
#include "vfs_objects/superblock.h"
#include "vfs_objects/file_descriptor.h"
#include "vfs_objects/open_file.h"
#include "vfs_objects/mount_table.h"
#include "../klib/path.h"
#include "../klib/string.h"

typedef enum vfs_lookup_method {
    VFS_LOOKUP_NORMAL,
    VFS_LOOKUP_PARENT
} vfs_lookup_method;

typedef struct vfs_lookup_result {
    file_descriptor_t *fd;         // when normal lookup
    file_descriptor_t *parent_dir; // when parent lookup
    const char *last_name;
} vfs_lookup_result;

static int vfs2_lookup(file_descriptor_t *start, const char *path, vfs_lookup_method method, vfs_lookup_result *result) {
    if (path == NULL || *path == 0 || path[0] != '/')
        return ERR_BAD_ARGUMENT;
    if (mtab.get_entries_list() == NULL)
        return ERR_NO_FS_MOUNTED;

    file_descriptor_t *curr = file_descriptors.clone(start);
    file_descriptor_t *next = NULL;
    mount_entry_t *mte;

    char path_part[256];
    int part_start = 0;
    int path_len = strlen(path);
    int err;

    if (path[part_start] == '/') {
        curr = mtab.get_entries_list()->root_dir;
        part_start++;
    }

    while (part_start < path_len) {

        // see if we are looking for parent dir and we are done
        if (method == VFS_LOOKUP_PARENT && strchr(path + part_start, '/') == 0) {
            result->parent_dir = curr;
            result->last_name = path + part_start;
            return OK;
        }

        // else, we need to continue the path
        err = get_next_path_part(path, &part_start, path_part);
        if (err) return err;

        if (path_part[0] == 0 || strcmp(path_part, ".") == 0)
            continue;
        
        if (strcmp(path_part, "..") == 0) {
            // we may cross a mount point
            mte = mtab.find_entry_by_root_dir(curr);
            if (mte != NULL && mte->host_dir != NULL) {
                file_descriptors.destroy(curr);
                curr = file_descriptors.clone(mte->host_dir);
            }
            // fallback into leaving the fs driver find the ".." entry
        }

        if (!file_descriptors.is_dir(curr))
            return ERR_NOT_A_DIRECTORY;
        
        err = curr->sb->driver->lookup(curr, path_part, &next);
        if (err) return err;

        // we may cross a mount point
        mte = mtab.find_entry_by_host_dir(next);
        if (mte != NULL) {
            file_descriptors.destroy(next);
            next = file_descriptors.clone(mte->root_dir);
        }

        file_descriptors.destroy(curr);
        curr = next;
    }

    result->fd = curr;
    return OK;
}

static int vfs2_lookup_target(const char *path, file_descriptor_t **target_out) {
    if (path[0] != '/') return ERR_BAD_ARGUMENT; // till we get process cwd

    vfs_lookup_result result;
    int err = vfs2_lookup(NULL, path, VFS_LOOKUP_NORMAL, &result);
    if (err) return err;

    *target_out = result.fd;
    return OK;
}

static int vfs2_lookup_parent(const char *path, file_descriptor_t **parent_out, const char **final_name) {
    if (path[0] != '/') return ERR_BAD_ARGUMENT; // till we get process cwd

    vfs_lookup_result result;
    int err = vfs2_lookup(NULL, path, VFS_LOOKUP_PARENT, &result);
    if (err) return err;

    *parent_out = result.parent_dir;
    *final_name = result.last_name;
    return OK;
}


// ----------------------------------------------------------------------------------------

int vfs2_mount(const char *path, block_device_t *dev, fs_driver_ops_t *driver) {
    int err;
    file_descriptor_t *host_dir;

    if (strcmp(path, "/") == 0) {
        // mount without parent
        if (mtab.get_entries_list() != NULL)
            return ERR_DIR_HAS_MOUNT;
        host_dir = NULL;

    } else {
        err = vfs2_lookup_target(path, &host_dir);
        if (err != OK) return err;

        if (!file_descriptors.is_dir(host_dir))
            return ERR_NOT_A_DIRECTORY;

        mount_entry_t *me = mtab.find_entry_by_host_dir(host_dir);
        if (me != NULL) return ERR_DIR_HAS_MOUNT;
    }

    superblock_t *sb = superblocks.create(driver, dev);
    err = driver->mount(sb);
    if (err) return err;

    file_descriptor_t *new_root_dir;
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
    file_descriptor_t *dir;

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
    file_descriptor_t *fd;

    err = vfs2_lookup_target(path, &fd);
    if (err) return err;
    if (!file_descriptors.is_file(fd))
        return ERR_NOT_A_FILE;

    err = fd->sb->driver->open(fd, flags, file);
    if (err) return err;

    // cache size for offset calculations
    (*file)->size = fd->size;
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
    ssize_t bytes = file->sb->driver->read(file, buf, len);
    if (bytes < 0) // negative numbers are errors
        return bytes;
    
    // do we need copy-to-user?
    file->offset += bytes;
    return bytes;
}

ssize_t vfs2_write(open_file_t *file, const void *buf, size_t len) {
    ssize_t bytes = file->sb->driver->write(file, buf, len);
    if (bytes < 0) // negative numbers are errors
        return bytes;
    // do we need to update the offset and file size?

    // do we need copy-from-user?

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

int vfs2_flush(open_file_t *file) {
    int err = file->sb->driver->flush(file);
    if (err) return err;

    return OK;
}

int vfs2_opendir(const char *path, open_file_t **dir) {
    int err;
    file_descriptor_t *fd;

    err = vfs2_lookup_target(path, &fd);
    if (err) return err;
    if (!file_descriptors.is_dir(fd))
        return ERR_NOT_A_DIRECTORY;

    err = fd->sb->driver->opendir(fd, dir);
    if (err) return err;

    // cache size for offset calculations
    (*dir)->size = fd->size;
    (*dir)->offset = 0;

    return OK;
}

int vfs2_readdir(open_file_t *dir, struct dirent *out) {
    // to update after the break... 
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_rewinddir(open_file_t *dir) {
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_closedir(open_file_t *dir) {
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_stat(const char *path, struct stat *out) {
    // metadata ops (resolve path or FD, call driver stat/truncate)
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_fstat(open_file_t *file, struct stat *out) {
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_truncate(const char *path, size_t size) {
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_create(const char *path, int type) {
    // creation/removal (resolve parent directory, extract final component name, call driver create/unlink/mkdir/rmdir)
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_unlink(const char *path) {
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_mkdir(const char *path) {
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_rmdir(const char *path) {
    return ERR_NOT_IMPLEMENTED;
}

