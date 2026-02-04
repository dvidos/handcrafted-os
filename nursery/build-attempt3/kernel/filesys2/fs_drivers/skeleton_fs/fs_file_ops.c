#include "../fs_driver_ops.h"
#include "internal.h"
#include "../../../include/uapi/errors.h"


int _skeleton_fs_open(file_descriptor_t *fd, int flags, open_file_t *file) {
    // create private data, store in file->driver_priv_data
    return ERR_NOT_IMPLEMENTED;
}

int _skeleton_fs_close(open_file_t *file) {
    // destroy private data from file->driver_priv_data
    return ERR_NOT_IMPLEMENTED;
}

int _skeleton_fs_read(open_file_t *file, void *buf, size_t len) {
    // grab private data and offset from file
    return ERR_NOT_IMPLEMENTED;
}

int _skeleton_fs_write(open_file_t *file, const void *buf, size_t len) {
    // grab private data and offset from file
    return ERR_NOT_IMPLEMENTED;
}

int _skeleton_fs_flush(open_file_t *file) {
    return ERR_NOT_IMPLEMENTED;
}


