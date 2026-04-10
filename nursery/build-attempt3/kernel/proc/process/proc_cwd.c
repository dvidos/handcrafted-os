#include "process.h"
#include "../../utils/assert.h"
#include "../../klib/string.h"
#include "../../memory/kheap.h"
#include "../../filesys/vfs_api.h"

MODULE("PROC_CWD", LOG_LEVEL_WARN);



error_t proc_getcwd(process_t *proc, char *buffer, int size) {
    if (size < strlen(proc->cwd_path) + 1)
        return ERR_NO_SPACE_LEFT;
    
    strcpy(buffer, proc->cwd_path);
    return OK;
}

// in unices, this would be called chdir(), especially in libc
error_t proc_chdir(process_t *proc, const char *path) {

    inode_t new_inode;
    error_t err = vfs_lookup_relative(proc->cwd_node, path, &new_inode);
    if (err) return err;
    if (!inodes.is_dir(&new_inode))
        return ERR_NOT_A_DIRECTORY;
    if (inodes.equals(&new_inode, &proc->cwd_node))
        return OK; // note, avoid string processing

    char *new_path = kmalloc(strlen(proc->cwd_path) + 1 + strlen(path) + 1);
    if (new_path == NULL)
        return traceable(ERR_NO_MEMORY);
    
    strcpy(new_path, proc->cwd_path);
    strcat(new_path, "/");
    strcat(new_path, path);
    vfs_canonicalize(new_path);
    
    if (proc->cwd_path != NULL)
        kfree(proc->cwd_path);
    proc->cwd_path = new_path;
    proc->cwd_node = new_inode;

    return OK;
}
