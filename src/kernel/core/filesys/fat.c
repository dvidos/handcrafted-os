#include <filesys/vfs.h>
#include <filesys/partition.h>

static int probe(struct partition *partition);

static struct file_system_driver vfs_driver = {
    .name = "FAT",
    .probe = probe
};

void fat_register_vfs_driver() {
    vfs_register_file_system_driver(&vfs_driver);
}

static int probe(struct partition *partition) {
    return -1;
}