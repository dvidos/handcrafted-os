#include "process.h"
#include "../../utils/assert.h"
#include "../../klib/string.h"
#include "../../memory/kheap.h"
#include "../../filesys/vfs_api.h"
#include "../../filesys/vfs_objects/mount_table.h"

MODULE("PROC_CWD", LOG_LEVEL_WARN);



error_t proc_getcwd(process_t *proc, char *buffer, int size) {
    log_trace("proc_getcwd(%p, %p, %d)", proc, buffer, size);
    if (size < strlen(proc->cwd_path) + 1)
        return ERR_NO_SPACE_LEFT;
    
    strcpy(buffer, proc->cwd_path);
    return OK;
}

error_t proc_chroot(process_t *proc, const char *path) {
    log_trace("proc_chroot(proc=%p, path=%s)", proc, path);

    ASSERT(proc != NULL);

    // this allows for the very first resolution
    if (strcmp(path, "/") == 0) {
        proc->vfs_ctx.root_inode = vfs_root_inode(&proc->vfs_ctx);
        return OK;
    }

    inode_t new_root_dir;
    error_t err = vfs_lookup(&proc->vfs_ctx, path, &new_root_dir);
    if (err) return err;
    if (!inodes.is_dir(&new_root_dir))
        return ERR_NOT_A_DIRECTORY;
    
    proc->vfs_ctx.root_inode = new_root_dir;
    return OK;
}

error_t proc_chdir(process_t *proc, const char *path) {
    log_trace("proc_chdir(proc=%p, path=%s)", proc, path);

    // note: proc->cwd_path can be null on the first call
    ASSERT(proc != NULL);
    ASSERT(!inodes.is_empty(&proc->vfs_ctx.root_inode)); // needed for resolution

    inode_t new_work_dir;
    error_t err = vfs_lookup(&proc->vfs_ctx, path, &new_work_dir);
    if (err) return err;
    if (!inodes.is_dir(&new_work_dir))
        return ERR_NOT_A_DIRECTORY;
    if (inodes.equals(&new_work_dir, &proc->vfs_ctx.cwd_inode))
        return OK; // note, avoid string processing

    char *new_path = kmalloc(strlen(proc->cwd_path) + 1 + strlen(path) + 1);
    if (new_path == NULL)
        return traceable(ERR_NO_MEMORY);
    
    new_path[0] = 0;
    if (proc->cwd_path != NULL)
        strcat(new_path, proc->cwd_path);
    strcat(new_path, "/");
    strcat(new_path, path);
    vfs_canonicalize(new_path);
    
    if (proc->cwd_path != NULL)
        kfree(proc->cwd_path);
    proc->cwd_path = new_path;
    proc->vfs_ctx.cwd_inode = new_work_dir;

    return OK;
}
