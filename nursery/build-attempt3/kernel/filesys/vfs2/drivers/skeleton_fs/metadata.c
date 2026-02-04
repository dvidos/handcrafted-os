#include "../vfs_contract.h"
#include "internal.h"
#include "../../../../include/uapi/errors.h"


int _skeleton_fs_create(file_descriptor_t *parent, const char *name, int type, file_descriptor_t *out) {
    return ERR_NOT_IMPLEMENTED;
}

int _skeleton_fs_unlink(file_descriptor_t *parent, const char *name) {
    return ERR_NOT_IMPLEMENTED;
}

int _skeleton_fs_stat(file_descriptor_t *fd, struct stat *out) {
    return ERR_NOT_IMPLEMENTED;
}

int _skeleton_fs_truncate(file_descriptor_t *fd, size_t size) {
    return ERR_NOT_IMPLEMENTED;
}

