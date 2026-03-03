#include "process.h"
#include "../../klib/string.h"


int proc_getcwd(process_t *proc, char *buffer, int size) {
    if (size < strlen(proc->curr_dir_path) + 1)
        return ERR_NO_SPACE_LEFT;
    // how is this updated when the folder is deleted from the filesystem?
    // maybe a message should be broadcasted to all processes?
    strcpy(buffer, proc->curr_dir_path);
    return OK;
}

// in unices, this would be called chdir(), especially in libc
int proc_chdir(process_t *proc, const char *path) {

    // if (vfs_get_root_mount() == NULL)
    //     return ERR_NO_FS_MOUNTED;
    
    // inode_t *root = vfs_get_root_mount()->mounted_fs_root;
    // inode_t *target = NULL;
    // int err = vfs_resolve(path, root, proc->curr_dir, false, &target);
    // if (err)
    //     return err;

    // if (proc->curr_dir != NULL)
    //     destroy_inode(proc->curr_dir);
    // proc->curr_dir = target;

    // if (proc->curr_dir_path != NULL)
    //     kfree(proc->curr_dir_path);
    // inode_get_full_path(proc->curr_dir, &proc->curr_dir_path);
    
    return OK;
}

