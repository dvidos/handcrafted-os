#include "vfs_api.h"
#include "../include/uapi/errors.h"
#include "fs_drivers/fs_driver_ops.h"
#include "vfs_objects/superblock.h"
#include "vfs_objects/file_descriptor.h"
#include "vfs_objects/open_file.h"
#include "vfs_objects/mount_table.h"
#include "../klib/path.h"
#include "../klib/string.h"


static int vfs2_lookup(file_descriptor_t *start, const char *path, file_descriptor_t **out) {
    if (path == NULL || *path == 0)
        return ERR_BAD_ARGUMENT;

    file_descriptor_t *curr = start;
    file_descriptor_t *next = NULL;
    mount_entry_t *mte;

    char path_part[256];
    int part_start = 0;
    int path_len = strlen(path);
    int err;

    if (path[part_start] == '/') {
        curr = mtab_entries_list_head->root_dir;
        part_start++;
    }

    while (part_start < path_len) {
        err = get_next_path_part(path, &part_start, path_part);
        if (err) return err;

        if (path_part[0] == 0 || strcmp(path_part, ".") == 0)
            continue;
        
        if (strcmp(path_part, "..") == 0) {
            // we may cross a mount point
            mte = mtab_find_by_root_dir(curr);
            if (mte != NULL && mte->host_dir != NULL)
                curr = mte->host_dir;
            // fallback into leaving the fs driver find the ".." entry
        }

        if ((curr->mode & S_IFMT) != S_IFDIR)
            return ERR_NOT_A_DIRECTORY;

        err = curr->sb->driver->lookup(curr, path_part, &next);
        if (err) return err;

        // we may cross a mount point
        mte = mtab_find_by_host_dir(next);
        if (mte != NULL) {
            next = mte->root_dir;
        }

        curr = next;
    }

    *out = curr;
    return OK;
}

// ----------------------------------------------------------------------------------------

int vfs2_mount(const char *path, block_device_t *dev, fs_driver_ops_t *driver) {
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_unmount(const char *path) {
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_sync(void) {
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_resolve(const char *path, file_descriptor_t *start, file_descriptor_t **out) {
    // path resolution (walks path components, handles . / .., crosses mount points, repeatedly calls lookup())
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_open(const char *path, int flags, int *out_fd) {
    // file open/close (resolve path -> file_descriptor_t, allocate open_file_t, call fd->sb->driver->open(fd, flags, open_file), store open_file_t in process FD table)
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_close(int fd) {
    return ERR_NOT_IMPLEMENTED;
}

ssize_t vfs2_read(int fd, void *buf, size_t len) {
    // i/o (validate FD, copy buffers if needed, call driver read/write/flush, offset lives in open_file_t)
    return ERR_NOT_IMPLEMENTED;
}

ssize_t vfs2_write(int fd, const void *buf, size_t len) {
    return ERR_NOT_IMPLEMENTED;
}

off_t vfs2_seek(int fd, off_t off, int whence) {
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_flush(int fd) {
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_opendir(const char *path, int *out_fd) {
    // directory operations (same FD table as files, type check (must be directory), driver handles iteration state in open_file_t))
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_readdir(int fd, struct dirent *out) {
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_rewinddir(int fd) {
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_closedir(int fd) {
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_stat(const char *path, struct stat *out) {
    // metadata ops (resolve path or FD, call driver stat/truncate)
    return ERR_NOT_IMPLEMENTED;
}

int vfs2_fstat(int fd, struct stat *out) {
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

