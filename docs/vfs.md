# Virtual File System

## this document

To clearly describe, but also use as reference, 
what we do for a Virtual File System (VFS) 
and how to extend it.

## layers participating

We modeled our file system layers losely to the layers of the Unix/Linux
VFS model. We have the following layers:

* storage devices (hard disks)
* partitions
* filesystems
* mount points
* file pointers and open file informations

### storage devices

From the kernel main, at some point 
during startup, we go exploring 
for storage devices.
This includes PCI devices, but also 
legacy ATA drives.

All devices discovered should be registered
using the `register_storage_device()` function.

The necessary functions for a storage device
are given below:

```c
struct storage_dev_ops {
    int (*sector_size)(struct storage_dev *dev);
    int (*read)(struct storage_dev *dev, uint32_t sector_low, uint32_t sector_hi, uint32_t sectors, char *buffer);
    int (*write)(struct storage_dev *dev, uint32_t sector_low, uint32_t sector_hi, uint32_t sectors, char *buffer);
};
```

### partitions

After all storage devices are discovered,
partition search is happening. We support
legacy FAT partition tables (4 primaries & extended), 
as well as the new GUID partition tables (up to 256 partitions).

When a partition has been identified, it is registered
using the function `register_partition()`. The partition 
structure has no function pointers, but has the following members:

```c
struct partition {
    char *name;               // a human friendly name
    uint8_t part_no;          // incremented number per device
    struct storage_dev *dev;  // the device this partition exists on
    uint32_t first_sector;
    uint32_t num_sectors;
    bool bootable;            // bootable flag (FAT and GTP)
    uint8_t legacy_type;      // FAT partition tables (e.g. 0x83 is linux)
};
```

### file system drivers

Various file systems can be implemented to support
files in partitions. Each file system should register itself,
using the `vfs_register_file_system_driver()` function. 
It must pass an instance of the following structure:

```c
struct file_system_driver {
    char *name;    // e.g. "FAT", "ext2" etc.

    // probe() looks at a partition to see if the stored format is supported
    int (*probe)(struct partition *partition);

    // get and kill superblock work for mounting / unmounting the partition
    struct super_block *(*get_superblock)(struct partition *partition);
    int (*kill_superblock)(struct super_block *);
};
```

The superblock represents the partition data in memory.
It is maintained for as long as the partition is mounted.
It's structure is given below:

```c
struct superblock {
    // returns file operations pointers for this partition
    struct file_ops *(*get_file_operations)();

    void *priv_data;  // data private to the filesystem driver
};
```

### mount a filesystem

At a later point, main tries to mount
the root filesystem, as well as any other
filesystems supported.

The desired partition to use for root 
filesystem can be given using the `root=dxpy`
option in the kernel command line, where x is the device no and y is the partition no.

If a driver supports the device, 
they must provide a structure with the 
supported operations. They must be the following:

```c
(something that reads superblock,
stores information,
opens root dir and returns a file_t struct)
```

All the active mounted filesystems are maintained in a linked 
list of mount information. Use the `vfs_mount()` and `vfs_unount()`
to mount and unmount filesystems. The information about a mount
point is given below:

```c
struct mount_info {
    struct storage_dev *dev;
    struct partition *part;
    struct file_system_driver *driver;
    struct super_block *superblock;

    char *path;
    struct file_ops *ops;
    .. something about superblock, maybe 
};
```

### root, cwd, entries and files

By having the ability to open the root 
directory of a filesystem, we can ask
it to resolve a path, into a "file pointer"
structure. This contains the location of 
the file on the disk.

Using this "file pointer", we can open 
the target file or dir. By opening a second 
directory as the "current" directory,
we can resolve both absolute paths (using the root dir), or the "current" directory,
which is kept in the memory of each process.

We can now resolve paths, get "file pointer"
structures, open the files to get "open file" structures. The "open file" structure
keeps track of file open mode and reading
or writing position.













## how to implement a file system

* minimum things to write
* using multiple source files, but keeping functions hidden
* how to use a "memory disk" to unit test your code

## see also

* https://unix.stackexchange.com/questions/4402/what-is-a-superblock-inode-dentry-and-a-file
* https://developer.ibm.com/tutorials/l-linux-filesystem/
* https://tldp.org/LDP/lki/lki-3.html


