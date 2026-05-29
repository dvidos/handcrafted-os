#include "open_file.h"
#include "../memory/kheap.h"
#include "../klib/string.h"

MODULE("OPEN_FILE", LOG_LEVEL_DEBUG);


static open_file_t *_open_file_create(superblock_t *sb, inode_t *n, int flags) {
    open_file_t *f = (open_file_t *)kmalloc(sizeof(open_file_t));

    f->sb = sb;     // superblock (operations)
    f->inode = *n;  // copy the value object
    f->offset = 0;  // VFS-owned file position
    f->flags = flags;   // RDONLY, WRONLY, APPEND, etc
    f->driver_priv_data = 0; // driver-specific open context
    f->lock = 0;    // protects offset & state

    f->refcount = 1; // for the function we'll return to.

    return f;
}

static void _open_file_hold(open_file_t *f) {
    // called by dup(), dup2(), fork(), ensures object is not fred while someone is using it
    if (f != NULL)
        f->refcount++;
}

static void _open_file_release(open_file_t *f) {
    // Called by close(fd) or when a process exits and frees all fds.
    // Only frees the actual object when last reference disappears.
    if (f == NULL) return;

    f->refcount--;
    if (f->refcount == 0) {
        // ..close inode, relase driver priv data...
        kfree(f);
    }
}


static void _open_file_log_formatter(log_write_stream_t *stream, va_list args) {
    open_file_t *f = va_arg(args, open_file_t *);
    stream->printf(stream, "inode.no=%llu, inode.size=%llu, offset=%llu, size=%llu",
        f->inode.inode_num,
        f->inode.size,
        f->offset,
        f->size
    );
}

struct open_file_ops open_files = {
    .create  = _open_file_create,
    .hold = _open_file_hold,
    .release = _open_file_release,
    .formatter = _open_file_log_formatter,
};
