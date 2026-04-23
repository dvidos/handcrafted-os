#include "../../filesys/dev_driver.h"
#include "../../klib/string.h"
#include "../../devices/tty/console_mgr.h"
#include "../../include/uapi/errors.h"
#include "../../include/uapi/ioctl.h"
#include "../../logger/logger.h"


MODULE("DEV_TTY", LOG_LEVEL_DEBUG);

static superblock_t tty_superblock;




static error_t tty_driver_open(inode_t *n, int flags, open_file_t **file_handle) {
    log_trace("tty_driver_open(minor=%d)", (int)n->inode_num);

    int dev_no = (int)n->inode_num;
    vconsole_t *vc = console_mgr_get_vconsole(dev_no);
    if (vc == NULL)
        return ERR_BAD_ARGUMENT;

    open_file_t *f = open_files.create(&tty_superblock, n);
    f->driver_priv_data = vc;

    *file_handle = f;
    return OK;
}

static error_t tty_driver_close(open_file_t *file) {
    log_trace("tty_driver_close(minor=%d)", (int)file->inode.inode_num);

    if (file->driver_priv_data == NULL)
        return ERR_BAD_ARGUMENT;
    vconsole_t *vc = (vconsole_t *)file->driver_priv_data;
    vc->ops->puts(vc, "(line closing)\n");

    open_files.release(file);
    return OK;
}

static ssize_t tty_driver_read(open_file_t *file, void *buf, size_t len, off_t offset) {
    log_trace("tty_driver_read(minor=%d, len=%u)", (int)file->inode.inode_num, len);

    if (file->driver_priv_data == NULL)
        return ERR_BAD_ARGUMENT;
    vconsole_t *vc = (vconsole_t *)file->driver_priv_data;

    int bytes = vc->ops->read(vc, buf, len);
    // log_info("vconsole.read() --> %d bytes, buf is '%s'", bytes, buf);
    return bytes;
}

static ssize_t tty_driver_write(open_file_t *file, const void *buf, size_t len, off_t offset) {
    log_trace("tty_driver_write(minor=%d, len=%u)", (int)file->inode.inode_num, len);

    if (file->driver_priv_data == NULL)
        return ERR_BAD_ARGUMENT;
    vconsole_t *vc = (vconsole_t *)file->driver_priv_data;

    vc->ops->write(vc, buf, len);
    return len;
}

static error_t tty_driver_ioctl(open_file_t *file, uint32_t cmd, long arg) {
    log_trace("tty_driver_ioctl(cmd=%u, arg=%ld)", cmd, arg);

    if (file->driver_priv_data == NULL)
        return ERR_BAD_ARGUMENT;
    vconsole_t *vc = (vconsole_t *)file->driver_priv_data;
    error_t err = OK;

    // TODO: at some point we must introduce copy_to_user(), copy_from_user()
    switch (cmd) {
        case TCGETS:
            return 1; // emulate 

        case TTY_GET_CANONICAL_MODE:
            *(bool *)arg = vc->ops->get_flag(vc, CANONICAL_MODE);
            break;
        case TTY_SET_CANONICAL_MODE:
            vc->ops->set_flag(vc, CANONICAL_MODE, (bool)arg);
            break;
        case TTY_GET_ECHO:
            *(bool *)arg = vc->ops->get_flag(vc, ECHO);
            break;
        case TTY_SET_ECHO:
            vc->ops->set_flag(vc, ECHO, (bool)arg);
            break;
        case TTY_GET_SIGNAL_HANDLING:
            *(bool *)arg = vc->ops->get_flag(vc, SIGNAL_HANDLING);
            break;
        case TTY_SET_SIGNAL_HANDLING:
            vc->ops->set_flag(vc, SIGNAL_HANDLING, (bool)arg);
            break;
        case TTY_GET_CR_TO_LF:
            *(bool *)arg = vc->ops->get_flag(vc, CR_TO_LF);
            break;
        case TTY_SET_CR_TO_LF:
            vc->ops->set_flag(vc, CR_TO_LF, (bool)arg);
            break;
        case TTY_GET_FLOW_CONTROL:
            *(bool *)arg = vc->ops->get_flag(vc, FLOW_CONTROL);
            break;
        case TTY_SET_FLOW_CONTROL:
            vc->ops->set_flag(vc, FLOW_CONTROL, (bool)arg);
            break;
        case TTY_GET_LF_TO_CRLF:
            *(bool *)arg = vc->ops->get_flag(vc, LF_TO_CRLF);
            break;
        case TTY_SET_LF_TO_CRLF:
            vc->ops->set_flag(vc, LF_TO_CRLF, (bool)arg);
            break;
        default:
            err = ERR_NOT_SUPPORTED;
    }

    return OK;
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
    .ioctl        = tty_driver_ioctl,
};

dev_driver_t tty_dev_driver = {
    .name = "TTY",
    .ops = &tty_fs_ops,
};

static superblock_t tty_superblock = {
    .driver = &tty_fs_ops,
};

