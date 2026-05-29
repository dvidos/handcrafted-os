#include "vfs_api.h"
#include "../include/uapi/errors.h"
#include "../include/uapi/vfs_seek_flags.h"
#include "../include/uapi/vfs_dirent.h"
#include "fs_driver.h"
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
#include "../include/uapi/vfs_file_flags.h" // For F_OK, R_OK, W_OK, X_OK and S_I* macros
#include "../include/uapi/vfs_stat.h"       // For vfs_stat_t

MODULE("VFS", LOG_LEVEL_INFO);


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


static error_t vfs_flex_lookup(vfs_context_t *ctx, const char *path, bool lookup_parent, inode_t *inod_out, const char **name_out) {
    // log_trace("vfs_flex_lookup(path='%s', parent=%s)", path, lookup_parent ? "true" : "false");
    ASSERT(ctx != NULL);
    ASSERT(inod_out != NULL);
    ASSERT(name_out != NULL);
    if (path == NULL || *path == 0)
        return traceable(ERR_BAD_ARGUMENT);

    inode_t curr = inodes.empty();
    inode_t next = inodes.empty();
    mount_entry_t *mte;
    char part_buffer[256];
    int part_offset = 0;
    int path_len = strlen(path);
    error_t err;

    // set starting point
    if (path[part_offset] == '/') {
        curr = ctx->root_inode;
        part_offset++;
    } else {
        curr = ctx->cwd_inode;
    }

    while (part_offset < path_len) {
        // log_debug("vfs_flex_lookup(), remaining='%s', curr=%d, next=%d", path + part_offset, curr.inode_num, next.inode_num);

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
        // log_debug("vfs_flex_lookup(), part='%s'", part_buffer);

        // skip over empty parts or same dir
        if (strlen(part_buffer) == 0 || strcmp(part_buffer, ".") == 0)
            continue;
        
        if (strcmp(part_buffer, "..") == 0) {
            // we may cross a mount point
            mte = ctx->mtab->ops->find_entry_by_root_dir(ctx->mtab, &curr);
            if (mte != NULL)
                curr = mte->host_dir;
            // fallback into leaving the fs driver find the ".." entry
        }

        if (!inodes.is_dir(&curr))
            return traceable(ERR_NOT_A_DIRECTORY);
        
        // Check execute permission on the current directory before traversing into it.
        error_t perm_err = vfs_permission(ctx, &curr, X_OK);
        if (perm_err) return perm_err;
        
        err = curr.sb->driver->lookup(&curr, part_buffer, &next);
        if (err) return traceable(err);

        // we may cross a mount point
        mte = ctx->mtab->ops->find_entry_by_host_dir(ctx->mtab, &next);
        if (mte != NULL)
            next = mte->root_dir;

        curr = next;
    }

    *inod_out = curr;
    *name_out = 0;
    return OK;
}

error_t vfs_lookup(vfs_context_t *ctx, const char *path, inode_t *target_out) {
    log_trace("vfs_lookup(path='%s')", path);
    ASSERT(ctx != NULL);
    ASSERT(path != NULL);
    ASSERT(target_out != NULL);

    const char *name_out;
    error_t err = vfs_flex_lookup(ctx, path, false, target_out, &name_out);
    if (err) return traceable(err);

    log_trace("vfs_lookup(path='%s') --> target_inode=%llu", path, target_out->inode_num);
    return OK;
}

static error_t vfs_lookup_parent(vfs_context_t *ctx, const char *path, inode_t *parent_out, const char **final_name_out) {
    log_trace("vfs_lookup_parent(path='%s')", path);
    ASSERT(ctx != NULL);
    ASSERT(path != NULL);
    ASSERT(parent_out != NULL);
    ASSERT(final_name_out != NULL);

    error_t err = vfs_flex_lookup(ctx, path, true, parent_out, final_name_out);
    if (err) return traceable(err);

    log_trace("vfs_lookup_parent(path='%s') --> parent_inode=%llu, name='%s'", path, parent_out->inode_num, *final_name_out);
    return OK;
}

void vfs_canonicalize(char *path) {
    ASSERT(path != NULL);

    // converts "/usr/../etc/./init" into "/etc/init"
    char *out = path; // write pointer
    char *in = path;  // read pointer

    while (*in) {
        if (*in == '/') {
            in++; 
            continue; 
        }

        // Check for "." and ".."
        if (in[0] == '.') {
            if (in[1] == '/' || in[1] == '\0') {
                in++; // Skip "."
                continue;
            }
            if (in[1] == '.' && (in[2] == '/' || in[2] == '\0')) {
                in += 2; // Skip ".."
                // Backtrack 'out' to previous '/'
                if (out > path + 1) {
                    out--; // move before trailing slash
                    while (out > path && *out != '/') out--;
                }
                continue;
            }
        }

        // Standard component: copy it
        *out++ = '/';
        while (*in && *in != '/') {
            *out++ = *in++;
        }
    }

    if (out == path)
        *out++ = '/'; // Ensure root is '/'
    *out = '\0';
}

// ----------------------------------------------------------------------------------------

error_t vfs_mount(vfs_context_t *ctx, const char *path, block_device_t *dev, fs_driver_ops_t *driver) {
    log_trace("vfs_mount(ctx=%p, path='%s', dev='%s')", ctx, path, dev->id);
    ASSERT(ctx != NULL);
    ASSERT(path != NULL);
    ASSERT(dev != NULL);
    ASSERT(driver != NULL);
    error_t err;
    inode_t host_dir = inodes.empty();

    if (strcmp(path, "/") == 0) {
        // mount without parent
        if (ctx->mtab->ops->get_entries_list(ctx->mtab) != NULL)
            return traceable(ERR_DIR_HAS_MOUNT);

    } else {
        err = vfs_lookup(ctx, path, &host_dir);
        if (err != OK) return traceable(err);

        if (!inodes.is_dir(&host_dir))
            return traceable(ERR_NOT_A_DIRECTORY);

        mount_entry_t *me = ctx->mtab->ops->find_entry_by_host_dir(ctx->mtab, &host_dir);
        if (me != NULL) return traceable(ERR_DIR_HAS_MOUNT);
    }

    superblock_t *sb = superblocks.create(driver, dev);
    err = driver->mount(sb);
    if (err) return traceable(err);

    inode_t new_root_dir;
    err = driver->get_root_dir(sb, &new_root_dir);
    if (err) return traceable(err);

    mount_entry_t *entry = ctx->mtab->ops->create_entry(&host_dir, &new_root_dir);
    err = ctx->mtab->ops->add_entry(ctx->mtab, entry);
    if (err) return traceable(err);

    // update VFS context, for the main use case
    if (inodes.is_empty(&ctx->root_inode))
        ctx->root_inode = entry->root_dir;
    if (inodes.is_empty(&ctx->cwd_inode))
        ctx->cwd_inode = entry->root_dir;
    

    // should release memory if failed
    return OK;
}

inode_t vfs_root_inode(vfs_context_t *ctx) {
    if (ctx->mtab == NULL || ctx->mtab->entries_list == NULL)
        panic("vfs_root_inode(), without any mounted filesystem");
    if (inodes.is_empty(&ctx->mtab->entries_list->root_dir))
        panic("vfs_root_inode(), but root inode is empty");

    return ctx->mtab->entries_list->root_dir;
}

error_t vfs_sync(vfs_context_t *ctx) {
    log_trace("vfs_sync()");
    for (mount_entry_t *entry = ctx->mtab->ops->get_entries_list(ctx->mtab); entry; entry = entry->next) {
        // ignore errors and try to sync all, anyway
        entry->sb->driver->sync(entry->sb);
    }
    return OK;
}

error_t vfs_unmount(vfs_context_t *ctx, const char *path) {
    log_trace("vfs_unmount(path='%s')", path);
    ASSERT(ctx != NULL);
    ASSERT(path != NULL);
    error_t err;
    inode_t dir = inodes.empty();

    err = vfs_lookup(ctx, path, &dir);
    if (err) return traceable(err);

    mount_entry_t *entry = ctx->mtab->ops->find_entry_by_root_dir(ctx->mtab, &dir);
    if (entry == NULL) return traceable(ERR_NOT_FOUND);

    err = entry->sb->driver->sync(entry->sb);
    if (err) return traceable(err);

    err = entry->sb->driver->unmount(entry->sb);
    if (err) return traceable(err);

    err = ctx->mtab->ops->remove_entry(ctx->mtab, entry);
    if (err) return traceable(err);
    
    ctx->mtab->ops->destroy_entry(entry);
    return OK;
}

static error_t vfs_open_device(const char *path, int flags, open_file_t **file) {
    log_trace("vfs_open_device(path='%s')", path);
    ASSERT(path != NULL);
    ASSERT(file != NULL);

    const device_t *dev = fs_lookup_device(path);
    if (dev == NULL)
        return traceable(ERR_NOT_FOUND);
    
    log_debug("device is #%d", dev->dev_number);
    inode_t dev_inode = { .inode_num = dev->dev_number };
    
    error_t err = dev->driver->ops->open(&dev_inode, flags, file);
    if (err) return traceable(err);

    if (dev->is_stream)
        (*file)->fmode |= FMODE_STREAM;

    return OK;
}

error_t vfs_open(vfs_context_t *ctx, const char *path, int flags, open_file_t **file) {
    log_trace("vfs_open(path='%s', flags=%d)", path, flags);
    ASSERT(ctx != NULL);
    ASSERT(path != NULL);
    ASSERT(file != NULL);
    error_t err;
    inode_t n = inodes.empty();
    inode_t parent_inode = inodes.empty();
    const char *final_name = NULL;
    bool created = false; // Flag to track if the file was newly created

    // ideally a /dev is mounted. for now we take a shortcut
    if (memcmp((char *)path, "/dev/", 5) == 0) {
        return vfs_open_device(path + 5, flags, file);
    }

    err = vfs_lookup(ctx, path, &n);

    if (err == ERR_NOT_FOUND) {
        if (flags & O_CREAT) {
            // File does not exist, and O_CREAT is set. Attempt to create.
            err = vfs_lookup_parent(ctx, path, &parent_inode, &final_name);
            if (err) return traceable(err);

            // Use S_IFREG for regular file type by default. Other types would need to be passed via mode.
            err = vfs_create(ctx, path, S_IFREG); 
            if (err) return traceable(err);
            created = true;
            // Now that the file is created, lookup its inode
            err = vfs_lookup(ctx, path, &n);
            if (err) return traceable(err); // Should not fail as it was just created
        } else {
            // File does not exist, and O_CREAT is not set. Return ERR_NOT_FOUND.
            return traceable(ERR_NOT_FOUND);
        }
    } else if (err == OK) {
        // File exists.
        if (flags & O_CREAT && flags & O_EXCL) {
            // O_CREAT and O_EXCL are set, but file already exists. Return ERR_FILE_EXISTS.
            return traceable(ERR_ALREADY_EXISTS);
        }
        if (!inodes.is_file(&n)) {
            // Path refers to a directory or other non-regular file.
            return traceable(ERR_NOT_A_FILE);
        }
    } else {
        // Some other lookup error occurred.
        return traceable(err);
    }

    // At this point, 'n' contains the inode of the file to open (either existing or newly created).

    // Determine requested access mode for the existing file
    int requested_access_mode = 0;
    if ((flags & O_ACCMODE) == O_RDONLY) {
        requested_access_mode = R_OK;
    } else if ((flags & O_ACCMODE) == O_WRONLY) {
        requested_access_mode = W_OK;
    } else if ((flags & O_ACCMODE) == O_RDWR) {
        requested_access_mode = R_OK | W_OK;
    }

    // If O_TRUNC is set, it implies write access
    if (flags & O_TRUNC) {
        requested_access_mode |= W_OK;
    }

    // Perform permission check if access modes (R/W/X) are requested
    if (!created && requested_access_mode != 0) { // No need to check permissions for newly created files, as they assume default permissions which are checked by creation.
        err = vfs_permission(ctx, &n, requested_access_mode);
        if (err) return traceable(err);
    }

    // Handle O_TRUNC
    if ((flags & O_TRUNC) && !created) { // Don't truncate if just created, as it's already empty
        ASSERT(n.sb != NULL);
        ASSERT(n.sb->driver != NULL);
        ASSERT(n.sb->driver->truncate != NULL);
        err = n.sb->driver->truncate(&n, 0);
        if (err) return traceable(err);
    }

    ASSERT(n.sb != NULL);
    ASSERT(n.sb->driver != NULL);
    ASSERT(n.sb->driver->open != NULL);
    err = n.sb->driver->open(&n, flags, file);
    if (err) return traceable(err);

    // Store the original flags in the open_file_t structure
    (*file)->flags = flags;
    // cache size for offset calculations
    (*file)->size = n.size;
    (*file)->offset = 0;

    // If O_APPEND is set, initial offset should be end of file
    if (flags & O_APPEND) {
        (*file)->offset = (*file)->size;
    }

    return OK;
}

error_t vfs_close(open_file_t *file) {
    log_trace("vfs_close(file=%ld)", file->inode);
    ASSERT(file != NULL);
    error_t err = file->sb->driver->close(file);
    if (err) return traceable(err);

    open_files.release(file);
    return OK;
}

ssize_t vfs_read(open_file_t *file, void *buf, size_t len) {
    log_trace("vfs_read(file=%ld, len=%d)", file->inode.inode_num, len);
    ASSERT(file != NULL);
    ASSERT(buf != NULL);

    // if file is write only, do not allow reading.
    if ((file->flags & O_ACCMODE) == O_WRONLY) {
        return ERR_NOT_PERMITTED;
    }

    ssize_t bytes = file->sb->driver->read(file, buf, len, file->offset);
    if (bytes < 0) // negative numbers are errors
        return bytes;
    
    if (!(file->fmode & FMODE_STREAM)) {
        file->offset += bytes;
    }
    return bytes;
}

ssize_t vfs_write(open_file_t *file, const void *buf, size_t len) {
    log_trace("vfs_write(file=%p, buff=%p, len=%d)", file, buf, len);
    ASSERT(file != NULL);
    ASSERT(buf != NULL);
    
    // if file is read only, do not allow writing.
    if ((file->flags & O_ACCMODE) == O_RDONLY) {
        return ERR_NOT_PERMITTED;
    }

    // streams don't track offsets or sizes
    if (!(file->fmode & FMODE_STREAM)) {
        // If O_APPEND is set, set the offset to the end of the file before writing
        if (file->flags & O_APPEND) {
            file->offset = file->size;
        }

        if (file->offset > file->size) {
            if (file->sb->driver->truncate)
                file->sb->driver->truncate(&file->inode, file->offset);
        }
    }

    ASSERT(file->sb != NULL);
    ASSERT(file->sb->driver != NULL);
    ASSERT(file->sb->driver->write != NULL);
    ssize_t bytes = file->sb->driver->write(file, buf, len, file->offset);
    if (bytes < 0) // negative numbers are errors
        return bytes;

    if (!(file->fmode & FMODE_STREAM)) {
        // maintain offset and size of seek
        file->offset += bytes;
        if (file->offset > file->size)
            file->size = file->offset;
    }

    return bytes;
}

off_t vfs_seek(open_file_t *file, off_t offset, int whence) {
    log_trace("vfs_seek(file=%llu, off=%ld, whence=%d)", file->inode.inode_num, offset, whence);
    ASSERT(file != NULL);
    ASSERT(whence == SEEK_SET || whence == SEEK_CUR || whence == SEEK_END);

    // streams (e.g. ttys) cannot seek
    if (file->fmode & FMODE_STREAM)
        return traceable(ERR_ILLEGAL_SEEK);

    off_t new_offset;
    switch (whence) {
        case SEEK_SET: new_offset = offset; break;
        case SEEK_CUR: new_offset = file->offset + offset; break;
        case SEEK_END: new_offset = file->size + offset; break;
        default: return traceable(ERR_BAD_ARGUMENT);
    }
    if (new_offset < 0) new_offset = 0;
    // if (new_offset > (off_t)file->size) new_offset = (off_t)file->size;  <-- we allow seek past EOF
    file->offset = new_offset;
    return new_offset;
}

error_t vfs_flush(open_file_t *file) {
    log_trace("vfs_flush(file=%ld)");
    ASSERT(file != NULL);
    error_t err = file->sb->driver->flush(file);
    if (err) return traceable(err);

    return OK;
}

error_t vfs_opendir(vfs_context_t *ctx, const char *path, open_file_t **dir) {
    log_trace("vfs_opendir(path='%s')", path);
    ASSERT(ctx != NULL);
    ASSERT(path != NULL);
    ASSERT(dir != NULL);
    error_t err;
    inode_t n = inodes.empty();

    err = vfs_lookup(ctx, path, &n);
    if (err) return traceable(err);
    if (!inodes.is_dir(&n))
        return traceable(ERR_NOT_A_DIRECTORY);

    // Check read and execute permissions on the directory
    err = vfs_permission(ctx, &n, R_OK | X_OK);
    if (err) return traceable(err);

    ASSERT(n.sb != NULL);
    ASSERT(n.sb->driver != NULL);
    ASSERT(n.sb->driver->opendir != NULL);
    err = n.sb->driver->opendir(&n, dir);
    if (err) return traceable(err);

    // cache size for offset calculations
    (*dir)->size = n.size;
    (*dir)->offset = 0;

    return OK;
}

ssize_t vfs_readdir(open_file_t *dir, vfs_dirent_t *out) {
    log_trace("vfs_readdir(dir=%ld) (size=%lu, offset=%lu)", dir->inode.inode_num, dir->size, dir->offset);
    ASSERT(dir != NULL);
    ASSERT(out != NULL);
    int bytes;

    ASSERT(dir->sb != NULL);
    ASSERT(dir->sb->driver != NULL);
    ASSERT(dir->sb->driver->readdir != NULL);
    bytes = dir->sb->driver->readdir(dir, out);
    if (bytes < 0) return bytes; // negative numbers are errors

    // The driver is now responsible for advancing dir->offset
    // based on its internal physical read position.
    // The bytes returned here signify one logical dirent entry was read.
    return bytes; // returning 0 at end, is more POSIX like
}

error_t vfs_rewinddir(open_file_t *dir) {
    log_trace("vfs_rewinddir(dir=%ld)", dir->inode.inode_num);
    ASSERT(dir != NULL);
    ASSERT(dir->sb->driver != NULL);
    ASSERT(dir->sb->driver->rewinddir != NULL);

    error_t err = dir->sb->driver->rewinddir(dir);
    if (err) return traceable(err);

    dir->offset = 0;
    return OK;
}

error_t vfs_closedir(open_file_t *dir) {
    log_trace("vfs_closedir(dir=%ld)", dir->inode.inode_num);
    ASSERT(dir != NULL);
    ASSERT(dir->sb->driver != NULL);
    ASSERT(dir->sb->driver->closedir != NULL);

    error_t err = dir->sb->driver->closedir(dir);
    if (err) return traceable(err);

    open_files.release(dir);
    return OK;
}

error_t vfs_stat(vfs_context_t *ctx, const char *path, vfs_stat_t *out) {
    log_trace("vfs_stat(path='%s')", path);
    ASSERT(ctx != NULL);
    ASSERT(path != NULL);
    ASSERT(out != NULL);

    inode_t n = inodes.empty();
    error_t err = vfs_lookup(ctx, path, &n);
    if (err) return traceable(err);

    ASSERT(n.sb != NULL);
    ASSERT(n.sb->driver != NULL);
    ASSERT(n.sb->driver->stat != NULL);

    err = n.sb->driver->stat(&n, out);
    if (err) return traceable(err);

    return OK;
}

error_t vfs_fstat(open_file_t *file, vfs_stat_t *out) {
    log_trace("vfs_fstat(file=%ld)", file->inode.inode_num);
    ASSERT(file != NULL);
    ASSERT(out != NULL);
    ASSERT(file->sb != NULL);
    ASSERT(file->sb->driver != NULL);
    ASSERT(file->sb->driver->stat != NULL);

    return file->sb->driver->stat(&file->inode, out);
}

// Check file access permissions
error_t vfs_access(vfs_context_t *ctx, const char *path, int mode) {
    log_trace("vfs_access(path='%s', mode=%d)", path, mode);
    ASSERT(ctx != NULL);
    ASSERT(path != NULL);

    inode_t n = inodes.empty();
    error_t err;

    err = vfs_lookup(ctx, path, &n);
    if (err) {
        // If lookup fails, it means the file does not exist, which is an error for all modes.
        return traceable(err);
    }

    // If only F_OK is requested and lookup succeeded, then the file exists.
    if (mode == F_OK) {
        return OK;
    }

    // Now call the centralized permission check
    return vfs_permission(ctx, &n, mode);
}

error_t vfs_permission(vfs_context_t *ctx, inode_t *n, int mode) {
    log_trace("vfs_permission(inode=%ld, mode=%d)", n->inode_num, mode);
    ASSERT(ctx != NULL);
    ASSERT(n != NULL);
    error_t err;

    vfs_stat_t stat_info;
    err = n->sb->driver->stat(n, &stat_info);
    if (err) {
        return traceable(err);
    }

    // Root (UID 0) always has access.
    if (ctx->uid == 0) {
        return OK;
    }

    // Determine which set of permissions to check (owner, group, or other) and build the required mask.
    uint32_t check_mask = 0;
    if (ctx->uid == stat_info.st_uid) {
        if (mode & R_OK) check_mask |= S_IRUSR;
        if (mode & W_OK) check_mask |= S_IWUSR;
        if (mode & X_OK) check_mask |= S_IXUSR;
    } else if (ctx->gid == stat_info.st_gid) {
        if (mode & R_OK) check_mask |= S_IRGRP;
        if (mode & W_OK) check_mask |= S_IWGRP;
        if (mode & X_OK) check_mask |= S_IXGRP;
    } else {
        if (mode & R_OK) check_mask |= S_IROTH;
        if (mode & W_OK) check_mask |= S_IWOTH;
        if (mode & X_OK) check_mask |= S_IXOTH;
    }

    // Check if all requested permissions (represented by check_mask) are set in the file's mode.
    if ((stat_info.st_mode & check_mask) == check_mask) {
        return OK;
    } else {
        return traceable(ERR_NOT_PERMITTED);
    }
}


error_t vfs_chmod(vfs_context_t *ctx, const char *path, uint32_t mode) {
    log_trace("vfs_chmod(path='%s', mode=%o)", path, mode);
    ASSERT(ctx != NULL);
    ASSERT(path != NULL);
    inode_t n = inodes.empty();
    error_t err;

    err = vfs_lookup(ctx, path, &n);
    if (err) return traceable(err);

    ASSERT(n.sb != NULL);
    ASSERT(n.sb->driver != NULL);
    ASSERT(n.sb->driver->stat != NULL);

    vfs_stat_t stat_info;
    err = n.sb->driver->stat(&n, &stat_info);
    if (err) return traceable(err);

    // Only root or owner can change permissions
    if (ctx->uid != 0 && ctx->uid != stat_info.st_uid) {
        return traceable(ERR_NOT_PERMITTED);
    }
    
    // Only permission bits are allowed to be set
    mode &= S_IRWXUGO;
    return n.sb->driver->chmod(&n, mode);
}

error_t vfs_fchmod(vfs_context_t *ctx, open_file_t *file, uint32_t mode) {
    log_trace("vfs_fchmod(ctx=%p, file=%ld, mode=%o)", ctx, file->inode.inode_num, mode);
    ASSERT(ctx != NULL);
    ASSERT(file != NULL);

    inode_t n = file->inode;
    error_t err;

    ASSERT(n.sb != NULL);
    ASSERT(n.sb->driver != NULL);
    ASSERT(n.sb->driver->stat != NULL);

    vfs_stat_t stat_info;
    err = n.sb->driver->stat(&n, &stat_info);
    if (err) return traceable(err);

    // Only root or owner can change permissions
    if (ctx->uid != 0 && ctx->uid != stat_info.st_uid) {
        return traceable(ERR_NOT_PERMITTED);
    }

    // Only permission bits are allowed to be set
    mode &= S_IRWXUGO;
    return file->sb->driver->chmod(&file->inode, mode);
}

error_t vfs_chown(vfs_context_t *ctx, const char *path, uid_t uid, gid_t gid) {
    log_trace("vfs_chown(path='%s', uid=%d, gid=%d)", path, uid, gid);
    ASSERT(ctx != NULL);
    ASSERT(path != NULL);
    inode_t n = inodes.empty();
    error_t err;

    err = vfs_lookup(ctx, path, &n);
    if (err) return traceable(err);

    ASSERT(n.sb != NULL);
    ASSERT(n.sb->driver != NULL);
    ASSERT(n.sb->driver->stat != NULL);
    
    vfs_stat_t stat_info;
    err = n.sb->driver->stat(&n, &stat_info);
    if (err) return traceable(err);

    // Only root can change ownership
    if (ctx->uid != 0) {
        return traceable(ERR_NOT_PERMITTED);
    }

    ASSERT(n.sb != NULL);
    ASSERT(n.sb->driver != NULL);
    ASSERT(n.sb->driver->chown != NULL);

    return n.sb->driver->chown(&n, uid, gid);
}

error_t vfs_fchown(vfs_context_t *ctx, open_file_t *file, uid_t uid, gid_t gid) {
    log_trace("vfs_fchown(ctx=%p, file=%ld, uid=%d, gid=%d)", ctx, file->inode.inode_num, uid, gid);
    ASSERT(ctx != NULL);
    ASSERT(file != NULL);

    inode_t n = file->inode;
    error_t err;

    ASSERT(n.sb != NULL);
    ASSERT(n.sb->driver != NULL);
    ASSERT(n.sb->driver->stat != NULL);

    vfs_stat_t stat_info;
    err = n.sb->driver->stat(&n, &stat_info);
    if (err) return traceable(err);

    // Only root can change ownership
    if (ctx->uid != 0) {
        return traceable(ERR_NOT_PERMITTED);
    }

    ASSERT(n.sb != NULL);
    ASSERT(n.sb->driver != NULL);
    ASSERT(n.sb->driver->chown != NULL);

    return file->sb->driver->chown(&n, uid, gid);
}

error_t vfs_truncate(vfs_context_t *ctx, const char *path, size_t size) {
    log_trace("vfs_truncate(file='%s')", path);
    ASSERT(ctx != NULL);
    ASSERT(path != NULL);

    inode_t n = inodes.empty();
    error_t err = vfs_lookup(ctx, path, &n);
    if (err) return traceable(err);

    ASSERT(n.sb != NULL);
    ASSERT(n.sb->driver != NULL);
    ASSERT(n.sb->driver->truncate!= NULL);

    err = n.sb->driver->truncate(&n, size);
    if (err) return traceable(err);

    return OK;
}

error_t vfs_create(vfs_context_t *ctx, const char *path, int type) {
    log_trace("vfs_create(path='%s', type=%d)", path, type);
    ASSERT(ctx != NULL);
    ASSERT(path != NULL);

    inode_t dir = inodes.empty();
    const char *name_ptr = NULL;
    error_t err = vfs_lookup_parent(ctx, path, &dir, &name_ptr);
    if (err) return traceable(err);

    // Check write and execute permissions on the parent directory
    err = vfs_permission(ctx, &dir, W_OK | X_OK);
    if (err) return traceable(err);

    ASSERT(dir.sb != NULL);
    ASSERT(dir.sb->driver != NULL);
    ASSERT(dir.sb->driver->create != NULL);

    inode_t new_file = inodes.empty();
    err = dir.sb->driver->create(&dir, name_ptr, type, ctx->uid, ctx->gid, &new_file);
    if (err) return traceable(err);

    return OK;
}

error_t vfs_unlink(vfs_context_t *ctx, const char *path) {
    log_trace("vfs_unlink(path='%s')", path);
    ASSERT(ctx != NULL);
    ASSERT(path != NULL);

    inode_t dir = inodes.empty();
    const char *name_ptr = NULL;
    error_t err = vfs_lookup_parent(ctx, path, &dir, &name_ptr);
    if (err) return traceable(err);

    // Check write and execute permissions on the parent directory
    err = vfs_permission(ctx, &dir, W_OK | X_OK);
    if (err) return traceable(err);

    ASSERT(dir.sb != NULL);
    ASSERT(dir.sb->driver != NULL);
    ASSERT(dir.sb->driver->unlink!= NULL);

    inode_t new_file = inodes.empty();
    err = dir.sb->driver->unlink(&dir, name_ptr);
    if (err) return traceable(err);

    return OK;
}

error_t vfs_mkdir(vfs_context_t *ctx, const char *path) {
    log_trace("vfs_mkdir(path='%s')", path);
    ASSERT(ctx != NULL);
    ASSERT(path != NULL);

    inode_t parent_dir = inodes.empty();
    const char *name_ptr = NULL;
    error_t err = vfs_lookup_parent(ctx, path, &parent_dir, &name_ptr);
    if (err) return traceable(err);

    // Check write and execute permissions on the parent directory
    err = vfs_permission(ctx, &parent_dir, W_OK | X_OK);
    if (err) return traceable(err);

    ASSERT(parent_dir.sb != NULL);
    ASSERT(parent_dir.sb->driver != NULL);
    ASSERT(parent_dir.sb->driver->mkdir != NULL);

    inode_t new_dir = inodes.empty();
    err = parent_dir.sb->driver->mkdir(&parent_dir, name_ptr, &new_dir);
    if (err) return traceable(err);

    return OK;
}

error_t vfs_rmdir(vfs_context_t *ctx, const char *path) {
    log_trace("vfs_rmdir(path='%s')", path);
    ASSERT(ctx != NULL);
    ASSERT(path != NULL);

    inode_t dir = inodes.empty();
    const char *name_ptr = NULL;
    error_t err = vfs_lookup_parent(ctx, path, &dir, &name_ptr);
    if (err) return traceable(err);

    // Check write and execute permissions on the parent directory
    err = vfs_permission(ctx, &dir, W_OK | X_OK);
    if (err) return traceable(err);

    ASSERT(dir.sb != NULL);
    ASSERT(dir.sb->driver != NULL);
    ASSERT(dir.sb->driver->rmdir != NULL);

    err = dir.sb->driver->rmdir(&dir, name_ptr);
    if (err) return traceable(err);

    return OK;
}

error_t vfs_ioctl(open_file_t *file, uint32_t cmd, long arg) {
    log_trace("vfs_ioctl(file=%ld)", file->inode.inode_num);
    ASSERT(file != NULL);
    ASSERT(file->sb != NULL);
    ASSERT(file->sb->driver != NULL);
    ASSERT(file->sb->driver->ioctl != NULL);

    if (file->sb->driver->ioctl == NULL)
        return traceable(ERR_NOT_SUPPORTED);
    
    return file->sb->driver->ioctl(file, cmd, arg);
}
