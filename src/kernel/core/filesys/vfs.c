#include <devices/storage_dev.h>
#include <filesys/vfs.h>
#include <filesys/partition.h>
#include <filesys/drivers.h>
#include <filesys/mount.h>
#include <memory/kheap.h>
#include <klib/string.h>
#include <klog.h>
#include <errors.h>

MODULE("VFS");



static int resolve_file_system_and_prepare_file_structure(char *path, file_t *file) {
    klog_trace("resolve_file_system_and_prepare_file_structure(\"%s\")", path);

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
    file->ops = mount->driver->get_file_operations();
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
    int err = resolve_file_system_and_prepare_file_structure(path, file);
    if (err) 
        return err;
    if (file->ops->opendir == NULL)
        return ERR_NOT_SUPPORTED;
    return file->ops->opendir(file->path, file);
}

int vfs_rewinddir(file_t *file) {
    klog_trace("vfs_rewinddir(file=0x%p)", file);
    if (file->ops->rewinddir == NULL)
        return ERR_NOT_SUPPORTED;
    return file->ops->rewinddir(file);
}

int vfs_readdir(file_t *file, struct dir_entry *dir_entry) {
    klog_trace("vfs_readdir(file=0x%p, entry=0x%p)", file, dir_entry);
    if (file->ops->readdir == NULL)
        return ERR_NOT_SUPPORTED;
    return file->ops->readdir(file, dir_entry);
}

int vfs_closedir(file_t *file) {
    klog_trace("vfs_closedir(file=0x%p)", file);
    if (file->ops->closedir == NULL)
        return ERR_NOT_SUPPORTED;
    int err = file->ops->closedir(file);
    return err;
}

int vfs_open(char *path, file_t *file) {
    int err = resolve_file_system_and_prepare_file_structure(path, file);
    if (err)
        return err;
    if (file->ops->open == NULL)
        return ERR_NOT_SUPPORTED;
    return file->ops->open(file->path, file);
}

int vfs_read(file_t *file, char *buffer, int bytes) {
    if (file->ops->read == NULL)
        return ERR_NOT_SUPPORTED;
    return file->ops->read(file, buffer, bytes);
}

int vfs_write(file_t *file, char *buffer, int bytes) {
    if (file->ops->write == NULL)
        return ERR_NOT_SUPPORTED;
    return file->ops->write(file, buffer, bytes);
}

int vfs_seek(file_t *file, int offset, enum seek_origin origin) {
    if (file->ops->seek == NULL)
        return ERR_NOT_SUPPORTED;
    return file->ops->seek(file, offset, origin);
}

int vfs_close(file_t *file) {
    if (file->ops->close == NULL)
        return ERR_NOT_SUPPORTED;
    return file->ops->close(file);
}

int vfs_touch(char *path) {
    file_t file;
    int err = resolve_file_system_and_prepare_file_structure(path, &file);
    if (err)
        return err;
    if (file.ops->touch == NULL)
        return ERR_NOT_SUPPORTED;
    return file.ops->touch(file.path, &file);
}

int vfs_mkdir(char *path) {
    file_t file;
    int err = resolve_file_system_and_prepare_file_structure(path, &file);
    if (err)
        return err;
    if (file.ops->mkdir == NULL)
        return ERR_NOT_SUPPORTED;
    return file.ops->mkdir(file.path, &file);
}

int vfs_unlink(char *path) {
    file_t file;
    int err = resolve_file_system_and_prepare_file_structure(path, &file);
    if (err)
        return err;
    if (file.ops->unlink == NULL)
        return ERR_NOT_SUPPORTED;
    return file.ops->unlink(file.path, &file);
}

