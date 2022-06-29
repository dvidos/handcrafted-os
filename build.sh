#!/bin/sh

# we have various targets:
# - libc.lib
# - kernel.bin
# - all the utils to be compiled and copied into the sysroot
# - hdd.img
# 
# then we can either:
# - copy hdd.img into a usb stick
# - run hdd.img on qemu

# stop on first failure
set -e


cd src/libc && make
cd ../..
cd src/kernel && make
cd ../..
cd src/user && make
cd ../..


# to make the images use something like the below
# ---------------------------------------------------
# dd if=/dev/zero of=~/file.img bs=1024k count=10     (10 MB file)
# losetup /dev/loopUserImg imgs/disk10mb-fat16.img    (loopImg allows all r/w/x)
# losetup --detach /dev/loopUserImg                   (allows simple user again.)
# mount /mnt/userImg                     (user mounts /etc/loop101p1, as defined in /etc/fstab)
# umount /mnt/userImg                    (user unmounts)
# cp sysroot/bin/* /mnt/userImg/bin      (copy compiled user programs)
# 

