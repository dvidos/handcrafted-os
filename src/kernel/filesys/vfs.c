#include <devices/storage_dev.h>
#include <filesys/vfs.h>
#include <filesys/partition.h>
#include <filesys/drivers.h>
#include <filesys/mount.h>
#include <memory/kheap.h>
#include <klib/string.h>
#include <multitask/process.h>
#include <klog.h>
#include <errors.h>
#include <klib/path.h>

MODULE("VFS");

/*
    see `namei()` for a very traditional conversion of a path name
    to a pointed file structure

    use a variable `dp` for directory pointer at any time.
    if path starts with '/' use root_dir (global var), else curr_dir.
    retrieve device maj/min from this dir
    call iget() [???]
        find an inode by dev and inumber

    loop until we walked all the path:
        verify dp type is directory and permissions exist
        collect path part up to the next '/' or '\0'
        search directory for this part, reading disk blocks as needed
        if not found return error
        if not more path, check permissions and return
        if more path, if we got a directory, use that as `dp` for the next loop
        if completed, call iput() [???] and return


    in the Linux device drivers, we see that the api for block 
    devices is "open(struct inode *p, struct filp *p2)"
    it says that inode structure already contains a pointer 
    to the storage device (or partition in our case),
    which contains pointer to the filesystem driver private data.

    same private data is set in the filp (file_t in our case)

    it seems that, when probing a partition, somehow 
    the driver will return the "super block" (usually of an ext2)
    which contains file ops, among other things.
    it also contains a pointer to the root "dentry"

    so, an inode is information about a file, in the sense of a
    directory entry, where it is located, traditionally it was an index
    in a table of file entries, while a file_t structure is information
    about an open file, e.g. mode (r/w), position, operations pointers etc.
*/

static void debug_dir_entry_t(dir_entry_t *dir_entry) {
    
    klog_debug("  dir_entry->superblock     : 0x%x", dir_entry->superblock);
    klog_debug("  dir_entry->short_name     : \"%s\"", dir_entry->short_name);
    klog_debug("  dir_entry->file_size      : %d", dir_entry->file_size);
    klog_debug("  dir_entry->location_in_dev: %d", dir_entry->location_in_dev);
    klog_debug("  dir_entry->flags.dir      : %d", dir_entry->flags.dir);
    klog_debug("  dir_entry->flags.label    : %d", dir_entry->flags.label);
    klog_debug("  dir_entry->flags.read_only: %d", dir_entry->flags.read_only);
    klog_debug("  dir_entry->created        : %04d-%02d-%02d %02d:%02d:%02d", 
        dir_entry->created.year, dir_entry->created.month, dir_entry->created.day,
        dir_entry->created.hours, dir_entry->created.minutes, dir_entry->created.seconds
    );
    klog_debug("  dir_entry->modified       : %04d-%02d-%02d %02d:%02d:%02d", 
        dir_entry->modified.year, dir_entry->modified.month, dir_entry->modified.day,
        dir_entry->modified.hours, dir_entry->modified.minutes, dir_entry->modified.seconds
    );
}


// similar to namei() in unix/linux
int vfs_resolve(const char *path, const file_descriptor_t *root_dir, const file_descriptor_t *curr_dir, bool containing_folder, file_descriptor_t **target) {
    klog_trace("vfs_resolve(\"%s\", root=0x%x, curr=0x%x, container=%d)",
        path, root_dir, curr_dir, (int)containing_folder);
    int err = SUCCESS;
    char *path_copy = NULL;
    char *final_path = NULL;
    
    // test edge cases first
    if (path == NULL)
        return ERR_BAD_ARGUMENT;
    if (*path == '\0')
        return ERR_BAD_ARGUMENT;
    if (root_dir == NULL)
        return ERR_NO_FS_MOUNTED;

    // duplicate to go though dirname (which modifies the string)
    // but also again, because we will tokenize (modify the string)
    // and dirname may return a constant strict (e.g. ".")
    path_copy = strdup(path);
    if (containing_folder)
        final_path = strdup(dirname(path_copy));
    else
        final_path = strdup(path_copy);

    if (strlen(final_path) == 1) {
        if (*final_path == '.') {
            if (curr_dir == NULL) {
                err = ERR_BAD_ARGUMENT;
                goto out;
            }
            *target = clone_file_descriptor(curr_dir);
            goto out;
        }
        else if (*final_path == '/') {
            *target = clone_file_descriptor(root_dir);
            goto out;
        }
    }

    // establish base dir
    file_descriptor_t *base_dir = NULL;
    if (*final_path == '/') {
        base_dir = clone_file_descriptor(root_dir);
        path++;
    } else {
        if (curr_dir == NULL)
            return ERR_BAD_ARGUMENT;
        base_dir = clone_file_descriptor(curr_dir);
    }
    
    // maybe we were given "/mnt/hdd2/dir/file.txt" and we are at the "hdd2"
    // how to see that this is a mounted file system?
    // so that we switch to the new superblock and the correct function pointers?
    // maybe each filesystem driver will maintain a list of mounts and directories,
    // e.g. a list of file_designator_t and mounts,
    // so that resolving such a directory, we are given the root directory
    // of the mounted file system.
    // but, to not implement this in every filesys driver, 
    // we must implement something on VFS level. 

    // TODO: we may need to lock here, to avoid reentrance 
    // from another task, as the strtok() is not thread safe?

    char *name = strtok(final_path, "/");
    while (true) {
        klog_trace("Looking for name \"%s\" in base directory \"%s\"", name, base_dir->name);

        // there's another part to look for, verify that 
        // we are searching inside a directory, not a file
        if ((base_dir->flags && FD_DIR) == 0) {
            err = ERR_NOT_A_DIRECTORY;
            goto out;
        }

        // we are now sure we are based in a directory.
        err = base_dir->superblock->ops->lookup(base_dir, name, target);
        if (err) goto out;

        // here we could translate for mounted filesystems.
        // one can think of a mount as two pairs of file descriptors:
        // one for the mount point directory of the host system,
        // one for the root directory of the hosted system.
        // so, if we got a descriptor pointing to one dir (e.g. fs A, "/mnt/hda")
        // we could substitute the other dir (fs B, "/")

        // let's check if we finished
        name = strtok(NULL, "/");
        if (name == NULL || strlen(name) == 0) {
            // we are done, target contains the... target, so free base_dir
            destroy_file_descriptor(base_dir);
            break;
        }

        // we will search again, so rebase on the new target.
        // lookup() will create a new file_descriptor each call
        // so, free our current, use the new one.
        destroy_file_descriptor(base_dir);
        base_dir = *target;
    }

    // we visited all levels, we should be ok.
    err = SUCCESS;
out:
    if (path_copy != NULL)
        kfree(path_copy);
    if (final_path != NULL)
        kfree(final_path);
    return err;
}

int vfs_open(char *path, file_t **file) {
    int err;

    if (vfs_get_root_mount() == NULL) {
        err = ERR_NO_FS_MOUNTED;
        goto out;
    }
    
    file_descriptor_t *target = NULL;
    file_descriptor_t *curr = running_process() == NULL ? NULL : running_process()->curr_dir;
    err = vfs_resolve(path, vfs_get_root_mount()->mounted_fs_root, curr, false, &target);
    if (err) goto out;
    if (target == NULL) {
        err = ERR_BAD_VALUE;
        goto out;
    }
    if ((target->flags & FD_FILE) == 0) {
        err = ERR_NOT_A_FILE;
        goto out;
    }

    klog_debug("vfs_open(), resolved descriptor follows");
    debug_file_descriptor(target, 0);

    if (target->superblock->ops->open == NULL)
        return ERR_NOT_SUPPORTED;
    err = target->superblock->ops->open(target, 0, file);

out:
    klog_trace("vfs_open(\"%s\") -> %d", path, err);
    return err;
}

int vfs_read(file_t *file, char *buffer, int bytes) {
    if (file->superblock->ops->read == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->read(file, buffer, bytes);
}

int vfs_write(file_t *file, char *buffer, int bytes) {
    if (file->superblock->ops->write == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->write(file, buffer, bytes);
}

int vfs_seek(file_t *file, int offset, enum seek_origin origin) {
    if (file->superblock->ops->seek == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->seek(file, offset, origin);
}

int vfs_flush(file_t *file) {
    if (file->superblock->ops->flush == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->flush(file);
}

int vfs_close(file_t *file) {
    if (file->superblock->ops->close == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->close(file);
}

int vfs_opendir(char *path, file_t **file) {
    klog_trace("vfs_opendir(path=\"%s\")", path);
    int err;

    if (vfs_get_root_mount() == NULL) {
        err = ERR_NO_FS_MOUNTED;
        goto out;
    }
    
    file_descriptor_t *target = NULL;
    file_descriptor_t *curr = running_process() == NULL ? NULL : running_process()->curr_dir;
    err = vfs_resolve(path, vfs_get_root_mount()->mounted_fs_root, curr, false, &target);
    if (err) goto out;
    if (target == NULL) {
        err = ERR_BAD_VALUE;
        goto out;
    }
    if ((target->flags & FD_DIR) == 0) {
        err = ERR_NOT_A_DIRECTORY;
        goto out;
    }

    klog_debug("vfs_opendir(), resolved descriptor follows");
    debug_file_descriptor(target, 0);

    if (target->superblock->ops->opendir == NULL)
        return ERR_NOT_SUPPORTED;
    err = target->superblock->ops->opendir(target, file);
out:
    klog_trace("vfs_opendir(\"%s\") -> %d", path, err);
    return err;
}

int vfs_rewinddir(file_t *file) {
    klog_trace("vfs_rewinddir(file=0x%p)", file);
    if (file->superblock->ops->rewinddir == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->rewinddir(file);
}

int vfs_readdir(file_t *file, file_descriptor_t **fd) {
    klog_trace("vfs_readdir(file=0x%p)", file);
    if (file->superblock->ops->readdir == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->readdir(file, fd);
}

int vfs_closedir(file_t *file) {
    klog_trace("vfs_closedir(file=0x%p)", file);
    if (file->superblock->ops->closedir == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->closedir(file);
}

int vfs_touch(char *path) {
    klog_trace("vfs_touch(path=\"%s\")", path);
    int err;
    char *copy = NULL;

    if (vfs_get_root_mount() == NULL) {
        err = ERR_NO_FS_MOUNTED;
        goto out;
    }
    
    file_descriptor_t *parent = NULL;
    file_descriptor_t *curr = running_process() == NULL ? NULL : running_process()->curr_dir;
    err = vfs_resolve(path, vfs_get_root_mount()->mounted_fs_root, curr, true, &parent);
    if (err) goto out;
    if (parent == NULL) {
        err = ERR_BAD_VALUE;
        goto out;
    }

    klog_debug("vfs_touch(), resolved descriptor follows");
    debug_file_descriptor(parent, 0);

    if (parent->superblock->ops->touch == NULL) {
        err = ERR_NOT_SUPPORTED;
        goto out;
    }
    
    copy = strdup(path);
    err = parent->superblock->ops->touch(parent, pathname(copy));

out:
    if (copy != NULL)
        kfree(copy);
    klog_trace("vfs_touch(\"%s\") -> %d", path, err);
    return err;
}

int vfs_unlink(char *path) {
    klog_trace("vfs_unlink(path=\"%s\")", path);
    int err;
    char *copy = NULL;

    if (vfs_get_root_mount() == NULL) {
        err = ERR_NO_FS_MOUNTED;
        goto out;
    }
    
    file_descriptor_t *parent = NULL;
    file_descriptor_t *curr = running_process() == NULL ? NULL : running_process()->curr_dir;
    err = vfs_resolve(path, vfs_get_root_mount()->mounted_fs_root, curr, true, &parent);
    if (err) goto out;
    if (parent == NULL) {
        err = ERR_BAD_VALUE;
        goto out;
    }

    klog_debug("vfs_unlink(), resolved descriptor follows");
    debug_file_descriptor(parent, 0);

    if (parent->superblock->ops->unlink == NULL) {
        err = ERR_NOT_SUPPORTED;
        goto out;
    }
    
    copy = strdup(path);
    err = parent->superblock->ops->unlink(parent, pathname(copy));

out:
    if (copy != NULL)
        kfree(copy);
    klog_trace("vfs_unlink(\"%s\") -> %d", path, err);
    return err;
}

int vfs_mkdir(char *path) {
    klog_trace("vfs_mkdir(path=\"%s\")", path);
    int err;
    char *copy = NULL;

    if (vfs_get_root_mount() == NULL) {
        err = ERR_NO_FS_MOUNTED;
        goto out;
    }
    
    file_descriptor_t *parent = NULL;
    file_descriptor_t *curr = running_process() == NULL ? NULL : running_process()->curr_dir;
    err = vfs_resolve(path, vfs_get_root_mount()->mounted_fs_root, curr, true, &parent);
    if (err) goto out;
    if (parent == NULL) {
        err = ERR_BAD_VALUE;
        goto out;
    }

    klog_debug("vfs_mkdir(), resolved descriptor follows");
    debug_file_descriptor(parent, 0);

    if (parent->superblock->ops->mkdir == NULL) {
        err = ERR_NOT_SUPPORTED;
        goto out;
    }
    
    copy = strdup(path);
    err = parent->superblock->ops->mkdir(parent, pathname(copy));

out:
    if (copy != NULL)
        kfree(copy);
    klog_trace("vfs_mkdir(\"%s\") -> %d", path, err);
    return err;
}

int vfs_rmdir(char *path) {
    klog_trace("vfs_rmdir(path=\"%s\")", path);
    int err;
    char *copy = NULL;

    if (vfs_get_root_mount() == NULL) {
        err = ERR_NO_FS_MOUNTED;
        goto out;
    }
    
    file_descriptor_t *parent = NULL;
    file_descriptor_t *curr = running_process() == NULL ? NULL : running_process()->curr_dir;
    err = vfs_resolve(path, vfs_get_root_mount()->mounted_fs_root, curr, true, &parent);
    if (err) goto out;
    if (parent == NULL) {
        err = ERR_BAD_VALUE;
        goto out;
    }

    klog_debug("vfs_vfs_rmdir(), resolved descriptor follows");
    debug_file_descriptor(parent, 0);

    if (parent->superblock->ops->rmdir == NULL) {
        err = ERR_NOT_SUPPORTED;
        goto out;
    }
    
    // must see if empty, if supported
    int dir_contents = 0;
    if (parent->superblock->ops->opendir != NULL
        && parent->superblock->ops->readdir != NULL 
        && parent->superblock->ops->closedir != NULL
    ) {
        klog_debug("vfs_rmdir() checking if dir is empty...");
        file_t *f;
        file_descriptor_t *fd;
        err = parent->superblock->ops->opendir(parent, &f);
        if (err) {
            klog_debug("error %d opendir() directory to count contents", err);
            goto out;
        }
        while (parent->superblock->ops->readdir(f, &fd) == SUCCESS) {
            if (strcmp(fd->name, ".") != 0 && strcmp(fd->name, "..") != 0)
                dir_contents++;
            destroy_file_descriptor(fd);
        }
        parent->superblock->ops->closedir(f);
    }
    if (dir_contents > 0) {
        err = ERR_DIR_NOT_EMPTY;
        goto out;
    }

    copy = strdup(path);
    err = parent->superblock->ops->rmdir(parent, pathname(copy));

out:
    if (copy != NULL)
        kfree(copy);
    klog_trace("vfs_rmdir(\"%s\") -> %d", path, err);
    return err;
}
