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


// essentially the same function as `namei()`, back in System V...
static int resolve_path_to_entry(char *path, dir_entry_t *entry) {
    klog_trace("resolve_path_to_entry(\"%s\")", path);

    file_t *dir = NULL;
    struct superblock *superblock = NULL;
    bool allocated_dir = false;
    bool opened_dir = false;

    int offset = 0;
    int pathlen = strlen(path);
    char *buffer = kmalloc(pathlen + 1); // to be sure parts fit.
    int err;

    // resolve where we start from
    if (path[offset] == '/') {
        if (vfs_get_root_mount() == NULL)
            return ERR_NO_FS_MOUNTED;
        dir = vfs_get_root_mount()->root_dir;
        offset++;
    } else {
        process_t *p = running_process();
        if (p == NULL)
            return ERR_NO_RUNNING_PROCESS;
        dir = &p->cwd;
    }
    allocated_dir = false;
    opened_dir = false; // because it's supposed to be open
    superblock = dir->superblock;
    
    klog_debug("A");

    while (offset < pathlen) {
        get_next_path_part(path, &offset, buffer);
        err = superblock->ops->find_entry(dir, buffer, entry);
        if (err) {
            klog_error("Error %d finding entry %s in dir", err, buffer);
            goto out;
        }

        // if no more path exists, it means the entry is what we were looking for!
        if (offset >= strlen(path)) {
            if (opened_dir) {
                superblock->ops->closedir(dir);
                kfree(dir);
            }
            err = SUCCESS;
            goto out;
        }

        // Question: what will happen if path is ".." 
        // and we should move into another superblock?
        // we will have to solve this at some point...


        // since we have more path, we need to open this entry as a directory.
        // validate this is a directory.
        if (!entry->flags.dir) {
            err = ERR_NOT_A_DIRECTORY;
            goto out;
        }

        if (opened_dir) {
            err = superblock->ops->closedir(dir);
            if (err) goto out;
            kfree(dir);
        }

        dir = kmalloc(sizeof(file_t));
        memset(dir, 0, sizeof(file_t));
        allocated_dir = true;

        err = superblock->ops->opendir(entry, dir);
        if (err) goto out;
        opened_dir = true;
    }

out:
    if (buffer != NULL)
        kfree(buffer);
    if (dir != NULL && allocated_dir) {
        if (opened_dir)
            superblock->ops->closedir(dir);
        kfree(dir);
    }
    return err;
}




static int resolve_file_system_and_prepare_file_structure(char *path, file_t *file) {
    klog_trace("resolve_file_system_and_prepare_file_structure(\"%s\")", path);
    return ERR_NOT_IMPLEMENTED;

    // every path much be relative to a directory,
    // even the absolute paths are relative to the root directory.
    // it seems we need to keep the root directory of each mount opened,
    // so that we can resolve paths relative to them.
    // least the root directory of the whole filesytem must be loaded upon mount.


    // in theory, we would parse the path to find the mount point and mounted path
    // but for now, let's assume only one mounted system
    struct mount_info *mount = vfs_get_mounts_list();
    if (mount == NULL)
        return ERR_NOT_FOUND;

    // let's go with the second mount for now.
    if (mount->next == NULL)
        return ERR_NOT_FOUND;
    mount = mount->next;

    // we need to fill in the file_t structure.
    file->storage_dev = mount->dev;
    file->partition = mount->part;
    file->driver = mount->driver;
//     file->superblock->ops = mount->driver->get_file_operations();
    klog_debug("Path \"%s\" resolved to device #%d (\"%s\"), partition #%d, file driver \"%s\"",
        path,
        file->storage_dev->dev_no,
        file->storage_dev->name,
        file->partition->part_no,
        file->driver->name
    );

    // normally, we should give the relative path, after the mount
    file->path = path;
    
    return NO_ERROR;    
}

int vfs_opendir(char *path, file_t *file) {
    klog_trace("vfs_opendir(path=\"%s\", file=0x%p)", path, file);

    if (strcmp(path, "/") == 0) {
        mount_info_t *mount = vfs_get_root_mount();
        if (mount == NULL)
            return ERR_NO_FS_MOUNTED;
        return mount->superblock->ops->open_root_dir(mount->superblock, file);
    }

    klog_error("Opening subdirs not supported yet! :-(");
    // // find entry_t based on root or current workding dir,
    // // call the appropriate opendir(), passing the entry_t
    // int err = resolve_file_system_and_prepare_file_structure(path, file);
    // if (err) 
    //     return err;
    // if (file->superblock->ops->opendir == NULL)
    //     return ERR_NOT_SUPPORTED;

    // // return file->superblock->ops->opendir(file->path, file);
    return ERR_NOT_SUPPORTED;
}

int vfs_rewinddir(file_t *file) {
    klog_trace("vfs_rewinddir(file=0x%p)", file);
    if (file->superblock->ops->rewinddir == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->rewinddir(file);
}

int vfs_readdir(file_t *file, struct dir_entry *dir_entry) {
    klog_trace("vfs_readdir(file=0x%p, entry=0x%p)", file, dir_entry);
    if (file->superblock->ops->readdir == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->readdir(file, dir_entry);
}

int vfs_closedir(file_t *file) {
    klog_trace("vfs_closedir(file=0x%p)", file);
    if (file->superblock->ops->closedir == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->closedir(file);
}

int vfs_open(char *path, file_t *file) {
    klog_trace("vfs_open(\"%s\")", path);
    // klog_error("error: vfs_open(\"%s\")", path);

    // int err = resolve_file_system_and_prepare_file_structure(path, file);
    // if (err)
    //     return err;
    // if (file->superblock->ops->open == NULL)
    //     return ERR_NOT_SUPPORTED;
    // return file->superblock->ops->open(file->path, file);
    return ERR_NOT_SUPPORTED;
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

int vfs_close(file_t *file) {
    if (file->superblock->ops->close == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->close(file);
}

int vfs_touch(char *path) {
    file_t file;
    int err = resolve_file_system_and_prepare_file_structure(path, &file);
    if (err)
        return err;
    if (file.superblock->ops->touch == NULL)
        return ERR_NOT_SUPPORTED;
    // return file.superblock->ops->touch(file.path, &file);
    return ERR_NOT_SUPPORTED;
}

int vfs_mkdir(char *path) {
    file_t file;
    int err = resolve_file_system_and_prepare_file_structure(path, &file);
    if (err)
        return err;
    if (file.superblock->ops->mkdir == NULL)
        return ERR_NOT_SUPPORTED;
    // return file.superblock->ops->mkdir(file.path, &file);
    return ERR_NOT_SUPPORTED;
}

int vfs_unlink(char *path) {
    file_t file;
    int err = resolve_file_system_and_prepare_file_structure(path, &file);
    if (err)
        return err;
    if (file.superblock->ops->unlink == NULL)
        return ERR_NOT_SUPPORTED;
    // return file.superblock->ops->unlink(file.path, &file);
    return ERR_NOT_SUPPORTED;
}

