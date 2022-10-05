/**
 * Including C files here, 
 * to allow methods to be static, yet allow for easy navigation of the code.
 * 
 * The "cinl" extension was chosen, to mean "C inline".
 * 
 * Exported methods will be present in the include file of the includes directory.
 */


#include "fat_priv.h"

MODULE("FAT");


#include "dir_entry.c"
#include "debug.c"
#include "clusters.c"
#include "fat_dir_ops.c"
#include "fat_file_ops.c"
#include "fat_vfs.c"

static struct file_system_driver vfs_driver = {
    .name = "FAT",
    .supported = fat_supported,
    .open_superblock = fat_open_superblock,
    .close_superblock = fat_close_superblock,
};

// this is the only public method. all the rest go through pointers
void fat_register_vfs_driver() {
    vfs_register_file_system_driver(&vfs_driver);
}
