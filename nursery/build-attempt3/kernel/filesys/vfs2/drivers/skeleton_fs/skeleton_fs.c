#include "../vfs_contract.h"


// mount.c
int _skeleton_fs_mount(superblock_t *sb);
int _skeleton_fs_unmount(superblock_t *sb);
int _skeleton_fs_sync(superblock_t *sb);

// lookup.c
int _skeleton_fs_get_root_dir(superblock_t *sb, file_descriptor_t *out);
int _skeleton_fs_lookup(file_descriptor_t *dir, const char *name, file_descriptor_t *out);

// file_ops.c
int _skeleton_fs_open(file_descriptor_t *fd, int flags, open_file_t *open_file);
int _skeleton_fs_close(open_file_t *open_file);
int _skeleton_fs_read(open_file_t *open_file, void *buf, size_t len);
int _skeleton_fs_write(open_file_t *open_file, const void *buf, size_t len);
int _skeleton_fs_flush(open_file_t *open_file);

// dir_ops.c
int _skeleton_fs_opendir(file_descriptor_t *dir, open_file_t *dir_handle);
int _skeleton_fs_readdir(open_file_t *dir_handle, file_descriptor_t *out);
int _skeleton_fs_rewinddir(open_file_t *dir_handle);
int _skeleton_fs_closedir(open_file_t *dir_handle);
int _skeleton_fs_mkdir(file_descriptor_t *parent, const char *name); // dirs have special create semantics
int _skeleton_fs_rmdir(file_descriptor_t *parent, const char *name); // dirs have special delete semantics

// metadata.c
int _skeleton_fs_create(file_descriptor_t *parent, const char *name, int type, file_descriptor_t *out);
int _skeleton_fs_unlink(file_descriptor_t *parent, const char *name);
int _skeleton_fs_stat(file_descriptor_t *fd, struct stat *out);
int _skeleton_fs_truncate(file_descriptor_t *fd, size_t size);


fs_driver_ops_t skeleton_fs_ops = {
    .mount        = _skeleton_fs_mount,
    .unmount      = _skeleton_fs_unmount,
    .sync         = _skeleton_fs_sync,
    .get_root_dir = _skeleton_fs_get_root_dir,
    .lookup       = _skeleton_fs_lookup,
    .open         = _skeleton_fs_open,
    .close        = _skeleton_fs_close,
    .read         = _skeleton_fs_read,
    .write        = _skeleton_fs_write,
    .flush        = _skeleton_fs_flush,
    .opendir      = _skeleton_fs_opendir,
    .readdir      = _skeleton_fs_readdir,
    .rewinddir    = _skeleton_fs_rewinddir,
    .closedir     = _skeleton_fs_closedir,
    .create       = _skeleton_fs_create,
    .unlink       = _skeleton_fs_unlink,
    .mkdir        = _skeleton_fs_mkdir,
    .rmdir        = _skeleton_fs_rmdir,
    .stat         = _skeleton_fs_stat,
    .truncate     = _skeleton_fs_truncate,
};
