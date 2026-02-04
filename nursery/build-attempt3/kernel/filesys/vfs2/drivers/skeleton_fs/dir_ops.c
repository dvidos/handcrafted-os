#include "../vfs_contract.h"
#include "internal.h"
#include "../../../../include/uapi/errors.h"


int _skeleton_fs_opendir(file_descriptor_t *dir, open_file_t *dir_handle) {
    return ERR_NOT_IMPLEMENTED;
}

int _skeleton_fs_readdir(open_file_t *dir_handle, file_descriptor_t *out) {
    return ERR_NOT_IMPLEMENTED;
}

int _skeleton_fs_rewinddir(open_file_t *dir_handle) {
    return ERR_NOT_IMPLEMENTED;
}

int _skeleton_fs_closedir(open_file_t *dir_handle) {
    return ERR_NOT_IMPLEMENTED;
}

int _skeleton_fs_mkdir(file_descriptor_t *parent, const char *name) { 
    // create directory, but also "." and ".."
    return ERR_NOT_IMPLEMENTED;
}

// dirs have special delete semantics
int _skeleton_fs_rmdir(file_descriptor_t *parent, const char *name) {
    // check if dir is empty or not.
    // remove "." and ".."
    return ERR_NOT_IMPLEMENTED;
}
