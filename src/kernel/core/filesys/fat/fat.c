/**
 * Including C files here, 
 * to allow methods to be static, yet allow for easy navigation of the code.
 * 
 * The "cinl" extension was chosen, to mean "C inline".
 * 
 * Exported methods will be present in the include file of the includes directory.
 */


#include "dir_entry.cinl"
#include "debug.cinl"
#include "clusters.cinl"
#include "file_ops.cinl"
#include "fat_vfs.cinl"

// this is the only public method. all the rest go through pointers
void fat_register_vfs_driver() {
    vfs_register_file_system_driver(&vfs_driver);
}
