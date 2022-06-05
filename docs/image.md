
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



