#include "process.h"
#include "../../klib/string.h"
#include "../../logger/logger.h"


MODULE("PROC_FILE", LOG_LEVEL_INFO);


static bool is_valid_handle(process_t *proc, int handle) {
    if (handle < 0 || handle >= MAX_FILE_HANDLES)
        return false;
    
    return proc->file_handles[handle] != NULL;
}

static int allocate_file_handle(process_t *proc, open_file_t *file, int target) {
    mutex_acquire(&proc->process_lock);

    int handle;
    if (target == -1) {
        handle = ERR_HANDLES_EXHAUSTED;
        for (int i = 0; i < MAX_FILE_HANDLES; i++) {
            if (proc->file_handles[i] == NULL) {
                handle = i;
                break;
            }
        }
    } else {
        if (!is_valid_handle(proc, target))
            handle = target;
        else
            handle = ERR_BAD_ARGUMENT;
    }

    if (handle >= 0 && handle < MAX_FILE_HANDLES) {
        proc->file_handles[handle] = file;
    }
    
    mutex_release(&proc->process_lock);
    return handle;
}

static int release_file_handle(process_t *proc, int handle) {
    error_t err = OK;

    mutex_acquire(&proc->process_lock);
    if (is_valid_handle(proc, handle)) {
        open_files.release(proc->file_handles[handle]);
        proc->file_handles[handle] = NULL;
    } else {
        err = ERR_BAD_ARGUMENT;
    }
    mutex_release(&proc->process_lock);

     return err;
}

int proc_open(process_t *proc, char *name, int flags) {
    open_file_t *file;
    // fast resolve relative to cwd (we'll need rewinddir() at least)
    int err = vfs_open(&proc->vfs_ctx, name, flags, &file);
    if (err) return err;

    int handle = allocate_file_handle(proc, file, -1);
    return handle;
}

int proc_read(process_t *proc, int handle, char *buffer, int length) {
    if (!is_valid_handle(proc, handle)) {
        log_warn("error proc pid %d, gave bad handle %d on read()", proc->pid, handle);
        return ERR_BAD_ARGUMENT;
    }
    return vfs_read(proc->file_handles[handle], buffer, length);
}

int proc_write(process_t *proc, int handle, char *buffer, int length) {
    log_trace("proc_write(proc=0x%x, hndl=%d, buff=0x%p, len=%d)", proc, handle, buffer, length);

    if (!is_valid_handle(proc, handle)) {
        log_warn("error proc pid %d, gave bad handle %d on write()", proc->pid, handle);
        return ERR_BAD_ARGUMENT;
    }

    return vfs_write(proc->file_handles[handle], buffer, length);
}

int proc_seek(process_t *proc, int handle, int offset, int whence) {
    if (!is_valid_handle(proc, handle))
        return ERR_BAD_ARGUMENT;
    return vfs_seek(proc->file_handles[handle], offset, whence);
}

int proc_close(process_t *proc, int handle) {
    if (!is_valid_handle(proc, handle))
        return ERR_BAD_ARGUMENT;
    int err = vfs_close(proc->file_handles[handle]);
    if (err) return err;

    release_file_handle(proc, handle);
    return OK;
}

int proc_opendir(process_t *proc, char *name) {
    open_file_t *file;
    // fast resolve relative to cwd
    int err = vfs_opendir(&proc->vfs_ctx, name, &file);
    if (err) return err;

    int handle = allocate_file_handle(proc, file, -1);
    log_trace("proc_opendir() -> %d", handle);
    log_debug("Process handles table follows");
    log_debug_hex((void *)proc->file_handles, sizeof(open_file_t) * MAX_FILE_HANDLES, 0);
    return handle;
}

int proc_readdir(process_t *proc, int handle, vfs_dirent_t *entry) {
    if (!is_valid_handle(proc, handle))
        return ERR_BAD_ARGUMENT;
    error_t err = vfs_readdir(proc->file_handles[handle], entry);
    return err;
}

int proc_rewinddir(process_t *proc, int handle) {
    if (!is_valid_handle(proc, handle))
        return ERR_BAD_ARGUMENT;
    return vfs_rewinddir(proc->file_handles[handle]);
}

int proc_closedir(process_t *proc, int handle) {
    if (!is_valid_handle(proc, handle))
        return ERR_BAD_ARGUMENT;

    int err = vfs_closedir(proc->file_handles[handle]);
    if (err) return err;

    release_file_handle(proc, handle);
    return OK;
}

int proc_dup(process_t *proc, int fd) {
    if (!is_valid_handle(proc, fd))
        return ERR_BAD_ARGUMENT;
    
    int handle = allocate_file_handle(proc, proc->file_handles[fd], -1);

    if (is_valid_handle(proc, handle)) {
        // more than one owner
        open_files.hold(proc->file_handles[handle]);
    }

    return handle;
}

int proc_dup2(process_t *source_proc, int source_fd, process_t *target_proc, int target_fd) { 

    // if exists, release / close it.
    if (is_valid_handle(target_proc, target_fd))
        release_file_handle(target_proc, target_fd);

    // then duplicate into it
    int target_handle = allocate_file_handle(target_proc, source_proc->file_handles[source_fd], target_fd);
    if (target_handle < 0) return (error_t)target_handle;

    // more than one owner
    open_files.hold(target_proc->file_handles[target_handle]); 

    return target_handle;
}

int proc_pipe(process_t *proc, int fds[]) {
    // no idea what this does, yet.
    return ERR_NOT_IMPLEMENTED;
}

open_file_t *proc_get_open_file(process_t *proc, int handle) {
    if (!is_valid_handle(proc, handle))
        return NULL;
    return proc->file_handles[handle];
}

