#include <errors.h>
#include "vfs.h"
#include "partition.h"
#include "drivers.h"

static int supported(struct partition *partition);

static struct filesys_driver vfs_driver = {
    .name = "ext2",
    .supported = supported
};

void ext2_register_vfs_driver() {
    vfs_register_filesys_driver(&vfs_driver);
}

static int supported(struct partition *partition) {
    return ERR_NOT_IMPLEMENTED;
}