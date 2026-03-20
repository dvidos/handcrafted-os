#include "dev_tty.h"
#include "../../../klib/string.h"
#include "../../../devices/tty.h"
#include "../../../include/uapi/errors.h"
#include "../../../logger/logger.h"


MODULE("DEV_TTY", LOG_LEVEL_TRACE);

static superblock_t tty_superblock;


static error_t tty_driver_open(inode_t *n, int flags, open_file_t **file_handle) {
    log_trace("tty_driver_open(minor=%d)", (int)n->inode_num);

    if (n->inode_num >= (unsigned)tty_manager_get_devices_count())
        return ERR_BAD_ARGUMENT;
    
    open_file_t *f = open_files.create(&tty_superblock, n);
    f->driver_priv_data = tty_manager_get_device(n->inode_num);

    *file_handle = f;
    return OK;
}

static error_t tty_driver_close(open_file_t *file) {
    log_trace("tty_driver_close(minor=%d)", (int)file->inode.inode_num);

    if (file->driver_priv_data == NULL)
        return ERR_BAD_ARGUMENT;
    tty_t *tty = (tty_t *)file->driver_priv_data;

    const char *str = "(tty closing)\n";
    tty_write_specific_tty(tty, str, strlen(str));

    open_files.destroy(file);
    return OK;
}

static ssize_t tty_driver_read(open_file_t *file, void *buf, size_t len, off_t offset) {
    log_trace("tty_driver_read(minor=%d, len=%u)", (int)file->inode.inode_num, len);

    if (file->driver_priv_data == NULL)
        return ERR_BAD_ARGUMENT;
    tty_t *tty = (tty_t *)file->driver_priv_data;

    int bytes = tty_read_specific_tty(tty, buf, len);
    return bytes;
}

static ssize_t tty_driver_write(open_file_t *file, const void *buf, size_t len, off_t offset) {
    log_trace("tty_driver_write(minor=%d, len=%u)", (int)file->inode.inode_num, len);

    if (file->driver_priv_data == NULL)
        return ERR_BAD_ARGUMENT;
    tty_t *tty = (tty_t *)file->driver_priv_data;

    tty_write_specific_tty(tty, buf, len);
    return len;
}


static fs_driver_ops_t tty_fs_ops = {
    .probe        = NULL,
    .mount        = NULL,
    .unmount      = NULL,
    .sync         = NULL,
    .mkfs         = NULL,
    .get_root_dir = NULL,
    .lookup       = NULL,
    .open         = tty_driver_open,
    .close        = tty_driver_close,
    .read         = tty_driver_read,
    .write        = tty_driver_write,
    .flush        = NULL,
    .opendir      = NULL,
    .readdir      = NULL,
    .rewinddir    = NULL,
    .closedir     = NULL,
    .create       = NULL,
    .unlink       = NULL,
    .mkdir        = NULL,
    .rmdir        = NULL,
    .stat         = NULL,
    .truncate     = NULL,
};

dev_driver_t tty_dev_driver = {
    .name = "TTY",
    .ops = &tty_fs_ops,
};

static superblock_t tty_superblock = {
    .driver = &tty_fs_ops,
};

