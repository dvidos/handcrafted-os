#include <errors.h>
#include <filesys/vfs.h>
#include <filesys/partition.h>
#include <filesys/drivers.h>

static int supported(struct partition *partition);

static struct file_system_driver vfs_driver = {
    .name = "ext2",
    .supported = supported
};

void ext2_register_vfs_driver() {
    vfs_register_file_system_driver(&vfs_driver);
}

static int supported(struct partition *partition) {
    return ERR_NOT_IMPLEMENTED;
}