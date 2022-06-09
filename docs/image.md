
in creating an image, we need dd, losetup and mkfs...
see https://wiki.osdev.org/Loopback_Device

1. creating the image file using dd and /dev/zero
2. associating the image file with a device using losetup
3. creating a file system using mke2fs or similar, using the loopback device
4. mounting the loopback device as a directory on our file tree
5. manipulating files in the image, using the mount point
6. sync, umount, detach, enjoy your image...



from the losetup man page:

```
    # dd if=/dev/zero of=~/file.img bs=1024k count=10
    # losetup --find --show ~/file.img
    /dev/loop0
    # mkfs -t ext2 /dev/loop0
    # mount /dev/loop0 /mnt
    ...
    # umount /dev/loop0
    # losetup --detach /dev/loop0
```

To create partition tables, do `fdisk /dev/loop8` etc.
To mount images with partitions, use the `-P` flag: `losetup -P file.img`.
This creates devices for the partitions as well, e.g. `/dev/loop9p1`, `/dev/loop9p2`.

The dos_disk.img file contains a disk with a legacy partition table, 2 primary partitions and one extended.
In the primary partition there is a FAT32 filesystem (despite beeing too much for only 512 KB)
In this filesystem there are two text files and three directories.

The linux_disk.img contains a UEFI GUID Partition Table with two partitions.
The first partition contains an ext2fs file system.
In there there are a few files and a few directories.


