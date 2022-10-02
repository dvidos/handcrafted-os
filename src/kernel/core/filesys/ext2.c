#include <filesys/vfs.h>
#include <filesys/partition.h>
#include <filesys/drivers.h>

static int probe(struct partition *partition);

static struct file_system_driver vfs_driver = {
    .name = "ext2",
    .probe = probe
};

void ext2_register_vfs_driver() {
    vfs_register_file_system_driver(&vfs_driver);
}

static int probe(struct partition *partition) {
    return -1;
}