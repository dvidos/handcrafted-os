#include "vfs_api.h"
#include "../include/uapi/errors.h"
#include "../include/uapi/vfs_seek_flags.h"
#include "../include/uapi/vfs_dirent.h"
#include "fs_drivers/fs_driver.h"
#include "vfs_objects/superblock.h"
#include "vfs_objects/inode.h"
#include "vfs_objects/open_file.h"
#include "vfs_objects/mount_table.h"
#include "../klib/path.h"
#include "../klib/string.h"
#include "../logger/logger.h"
#include "../utils/assert.h"
#include "../klib/strerror.h"
#include "../filesys/fs_api.h"

MODULE("VFS", LOG_LEVEL_DEBUG);


typedef struct substring {
    const void *ptr;
    unsigned len;
} substring_t;

static substring_t get_path_first_substring(const char *path) {
    if (path == NULL || *path == 0)
        return (substring_t){.ptr = path, .len = 0};
    
    // skip initial slash
    if (*path == '/')
        path++;
    if (*path == 0)
        return (substring_t){.ptr = path, .len = 0};
    
    // find end slash, if any
    char *slash = strchr(path, '/');
    if (slash == 0)
        return (substring_t){.ptr = path, .len = strlen(path)};
    
    // return just the part, without the final slash
    return (substring_t){.ptr = path, .len = (slash - path)};
}


static error_t vfs_flex_lookup(inode_t *start, const char *path, bool lookup_parent, inode_t *inod_out, const char **name_out) {
    log_trace("vfs_flex_lookup(start=%llu, path='%s', parent=%s)", (start == NULL ? 0ULL : start->inode_num), path, lookup_parent ? "true" : "false");
    if (path == NULL || *path == 0 || path[0] != '/')
        return traceable(ERR_BAD_ARGUMENT);
    if (mtab.get_entries_list() == NULL)
        return traceable(ERR_NO_FS_MOUNTED);

    inode_t curr = start == NULL ? inodes.empty() : *start;
    inode_t next = inodes.empty();
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
        log_debug("vfs_flex_lookup(), remaining='%s', curr=%d, next=%d", path + part_offset, curr.inode_num, next.inode_num);

        // see if we are looking for parent dir and we are done
        if (lookup_parent && strchr(path + part_offset, '/') == 0) {
            *inod_out = curr;
            *name_out = path + part_offset;
            return OK;
        }

        // else, we need to continue the path
        substring_t ss = get_path_first_substring(path + part_offset);
        if (ss.len + 1 > sizeof(part_buffer))
            return traceable(ERR_NAME_TOO_LONG);
        memcpy(part_buffer, ss.ptr, ss.len);
        part_buffer[ss.len] = 0;
        part_offset += strlen(part_buffer) + 1;
        log_debug("vfs_flex_lookup(), part='%s'", part_buffer);

        // skip over empty parts or same dir
        if (strlen(part_buffer) == 0 || strcmp(part_buffer, ".") == 0)
            continue;
        
        if (strcmp(part_buffer, "..") == 0) {
            // we may cross a mount point
            mte = mtab.find_entry_by_root_dir(&curr);
            if (mte != NULL)
                curr = mte->host_dir;
            // fallback into leaving the fs driver find the ".." entry
        }

        if (!inodes.is_dir(&curr))
            return traceable(ERR_NOT_A_DIRECTORY);
        
        err = curr.sb->driver->lookup(&curr, part_buffer, &next);
        if (err) return err;

        // we may cross a mount point
        mte = mtab.find_entry_by_host_dir(&next);
        if (mte != NULL)
            next = mte->root_dir;

        curr = next;
    }

    *inod_out = curr;
    *name_out = 0;
    return OK;
}

static error_t vfs_lookup_target(const char *path, inode_t *target_out) {
    log_trace("vfs_lookup_target(path='%s')", path);
    if (path[0] != '/') return traceable(ERR_BAD_ARGUMENT); // till we get process cwd

    const char *name_out;
    int err = vfs_flex_lookup(NULL, path, false, target_out, &name_out);
    if (err) return err;

    return OK;
}

static error_t vfs_lookup_parent(const char *path, inode_t *parent_out, const char **final_name_out) {
    log_trace("vfs_lookup_parent(path='%s')", path);
    if (path[0] != '/') return traceable(ERR_BAD_ARGUMENT); // till we get process cwd

    int err = vfs_flex_lookup(NULL, path, true, parent_out, final_name_out);
    if (err) return err;

    return OK;
}

// ----------------------------------------------------------------------------------------

error_t vfs_mount(const char *path, block_device_t *dev, fs_driver_ops_t *driver) {
    log_trace("vfs_mount(path='%s', dev='%s')", path, dev->id);
    int err;
    inode_t host_dir = inodes.empty();

    if (strcmp(path, "/") == 0) {
        // mount without parent
        if (mtab.get_entries_list() != NULL)
            return traceable(ERR_DIR_HAS_MOUNT);

    } else {
        err = vfs_lookup_target(path, &host_dir);
        if (err != OK) return err;

        if (!inodes.is_dir(&host_dir))
            return traceable(ERR_NOT_A_DIRECTORY);

        mount_entry_t *me = mtab.find_entry_by_host_dir(&host_dir);
        if (me != NULL) return traceable(ERR_DIR_HAS_MOUNT);
    }

    superblock_t *sb = superblocks.create(driver, dev);
    err = driver->mount(sb);
    if (err) return err;

    inode_t new_root_dir;
    err = driver->get_root_dir(sb, &new_root_dir);
    if (err) return err;

    mount_entry_t *entry = mtab.create_entry(&host_dir, &new_root_dir);
    err = mtab.add_entry(entry);
    if (err) return err;

    // should release memory if failed
    return OK;
}

error_t vfs_unmount(const char *path) {
    log_trace("vfs_unmount(path='%s')", path);
    int err;
    inode_t dir = inodes.empty();

    err = vfs_lookup_target(path, &dir);
    if (err) return err;

    mount_entry_t *entry = mtab.find_entry_by_host_dir(&dir);
    if (entry == NULL) return traceable(ERR_NOT_FOUND);

    err = entry->sb->driver->sync(entry->sb);
    if (err) return err;

    err = entry->sb->driver->unmount(entry->sb);
    if (err) return err;

    err = mtab.remove_entry(entry);
    if (err) return err;
    
    mtab.destroy_entry(entry);
    return OK;
}

error_t vfs_sync(void) {
    log_trace("vfs_sync()");
    for (mount_entry_t *entry = mtab.get_entries_list(); entry; entry = entry->next) {
        // ignore errors and try to sync all, anyway
        entry->sb->driver->sync(entry->sb);
    }
    return OK;
}

static error_t vfs_open_device(const char *path, int flags, open_file_t **file) {
    log_trace("vfs_open_device(path='%s')", path);

    const device_t *dev = fs_lookup_device(path);
    if (dev == NULL)
        return ERR_NOT_FOUND;
    
    log_debug("device is #%d", dev->dev_number);
    inode_t dev_inode = { .inode_num = dev->dev_number };
    return dev->driver->ops->open(&dev_inode, 0, file);
}

error_t vfs_open(const char *path, int flags, open_file_t **file) {
    log_trace("vfs_open(path='%s', flags=%d)", path, flags);
    int err;
    inode_t n = inodes.empty();

    // ideally a /dev is mounted. for now we take a shortcut
    if (memcmp((char *)path, "/dev/", 5) == 0) {
        return vfs_open_device(path + 5, flags, file);
    }

    err = vfs_lookup_target(path, &n);
    if (err) return err;
    if (!inodes.is_file(&n))
        return traceable(ERR_NOT_A_FILE);

    err = n.sb->driver->open(&n, flags, file);
    if (err) return err;

    // cache size for offset calculations
    (*file)->size = n.size;
    (*file)->offset = 0;

    return OK;
}

error_t vfs_close(open_file_t *file) {
    log_trace("vfs_close(file=%ld)", file->inode);
    int err = file->sb->driver->close(file);
    if (err) return err;

    open_files.destroy(file);
    return OK;
}

ssize_t vfs_read(open_file_t *file, void *buf, size_t len) {
    log_trace("vfs_read(file=%ld, len=%d)", file->inode.inode_num, len);
    ssize_t bytes = file->sb->driver->read(file, buf, len, file->offset);
    if (bytes < 0) // negative numbers are errors
        return bytes;
    
    // do we need copy-to-user?
    file->offset += bytes;
    return bytes;
}

ssize_t vfs_write(open_file_t *file, const void *buf, size_t len) {
    log_trace("vfs_write(file=%ld, len=%d)", file->inode.inode_num, len);
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

off_t vfs_seek(open_file_t *file, off_t offset, int whence) {
    log_trace("vfs_seek(file=%ld, off=%d, whence=%d)", file->inode.inode_num, offset, whence);
    off_t new_offset;
    switch (whence) {
        case SEEK_SET: new_offset = offset; break;
        case SEEK_CUR: new_offset = file->offset + offset; break;
        case SEEK_END: new_offset = file->size + offset; break;
        default: return traceable(ERR_BAD_ARGUMENT);
    }
    if (new_offset < 0) new_offset = 0;
    if (new_offset > (off_t)file->size) new_offset = (off_t)file->size;
    file->offset = new_offset;
    return new_offset;
}

error_t vfs_flush(open_file_t *file) {
    log_trace("vfs_flush(file=%ld)");
    int err = file->sb->driver->flush(file);
    if (err) return err;

    return OK;
}

error_t vfs_opendir(const char *path, open_file_t **dir) {
    log_trace("vfs_opendir(path='%s')", path);
    int err;
    inode_t n = inodes.empty();

    err = vfs_lookup_target(path, &n);
    if (err) return err;
    if (!inodes.is_dir(&n))
        return traceable(ERR_NOT_A_DIRECTORY);

    err = n.sb->driver->opendir(&n, dir);
    if (err) return err;

    // cache size for offset calculations
    (*dir)->size = n.size;
    (*dir)->offset = 0;

    return OK;
}

ssize_t vfs_readdir(open_file_t *dir, vfs_dirent_t *out) {
    log_trace("vfs_readdir(dir=%ld)", dir->inode.inode_num);
    int bytes;

    bytes = dir->sb->driver->readdir(dir, out);
    if (bytes < 0) return bytes; // negative numbers are errors

    // The driver is now responsible for advancing dir->offset
    // based on its internal physical read position.
    // The bytes returned here signify one logical dirent entry was read.
    return bytes; // returning 0 at end, is more POSIX like
}

error_t vfs_rewinddir(open_file_t *dir) {
    log_trace("vfs_rewinddir(dir=%ld)", dir->inode.inode_num);
    int err = dir->sb->driver->rewinddir(dir);
    if (err) return err;

    dir->offset = 0;
    return OK;
}

error_t vfs_closedir(open_file_t *dir) {
    log_trace("vfs_closedir(dir=%ld)", dir->inode.inode_num);
    int err = dir->sb->driver->closedir(dir);
    if (err) return err;

    open_files.destroy(dir);
    return OK;
}

error_t vfs_stat(const char *path, vfs_stat_t *out) {
    log_trace("vfs_stat(path='%s')", path);
    inode_t n = inodes.empty();
    int err = vfs_lookup_target(path, &n);
    if (err) return err;

    err = n.sb->driver->stat(&n, out);
    if (err) return err;

    return OK;
}

error_t vfs_fstat(open_file_t *file, vfs_stat_t *out) {
    log_trace("vfs_fstat(file=%ld)", file->inode.inode_num);
    return file->sb->driver->stat(&file->inode, out);
}

error_t vfs_truncate(const char *path, size_t size) {
    log_trace("vfs_truncate(file='%s')", path);
    inode_t n = inodes.empty();
    int err = vfs_lookup_target(path, &n);
    if (err) return err;

    err = n.sb->driver->truncate(&n, size);
    if (err) return err;

    return OK;
}

error_t vfs_create(const char *path, int type) {
    log_trace("vfs_create(path='%s', type=%d)", path, type);
    inode_t dir = inodes.empty();
    const char *name_ptr = NULL;
    int err = vfs_lookup_parent(path, &dir, &name_ptr);
    if (err) return err;

    inode_t new_file = inodes.empty();
    err = dir.sb->driver->create(&dir, name_ptr, type, &new_file);
    if (err) return err;

    return OK;
}

error_t vfs_unlink(const char *path) {
    log_trace("vfs_unlink(path='%s')", path);
    inode_t dir = inodes.empty();
    const char *name_ptr = NULL;
    int err = vfs_lookup_parent(path, &dir, &name_ptr);
    if (err) return err;

    inode_t new_file = inodes.empty();
    err = dir.sb->driver->unlink(&dir, name_ptr);
    if (err) return err;

    return OK;
}

error_t vfs_mkdir(const char *path) {
    log_trace("vfs_mkdir(path='%s')", path);
    inode_t parent_dir = inodes.empty();
    const char *name_ptr = NULL;
    int err = vfs_lookup_parent(path, &parent_dir, &name_ptr);
    if (err) return err;

    inode_t new_dir = inodes.empty();
    err = parent_dir.sb->driver->mkdir(&parent_dir, name_ptr, &new_dir);
    if (err) return err;

    return OK;
}

error_t vfs_rmdir(const char *path) {
    log_trace("vfs_rmdir(path='%s')", path);
    inode_t dir = inodes.empty();
    const char *name_ptr = NULL;
    int err = vfs_lookup_parent(path, &dir, &name_ptr);
    if (err) return err;

    err = dir.sb->driver->rmdir(&dir, name_ptr);
    if (err) return err;

    return OK;
}

