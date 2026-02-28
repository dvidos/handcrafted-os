#!/bin/bash

# based on all created artifacts inside ./build, create the OS image
# we should have various addresses and sizes passed into us?
# use the ./sfs/sfs cli tool to generate the image
set -e

. ./config.sh  # get build variables

IMG=os.img
#SFS=./tools/sfs/sfs1
SFS=./tools/sfs_img/sfs_img
SRC=build/kernel
ROOTFS=./build/rootfs

echo "Making file $IMG with size $DISK_IMAGE_SIZE_MB MB"

# create image with partition
PARTITION1_OFFSET=$(( $PARTITION_1_FIRST_SECTOR * $SECTOR_SIZE ))
$SFS create-img $IMG ${DISK_IMAGE_SIZE_MB}M $PARTITION1_OFFSET

# ensure generated section2 and kernel are within expectations
STAGE2_BIN_SIZE=$(( ($(stat -c %s $SRC/stage2.bin) + 1023) / 1024 ))
KERNEL_BIN_SIZE=$(( ($(stat -c %s $SRC/kernel.bin) + 1023) / 1024 ))
if (( STAGE2_BIN_SIZE > STAGE2_SIZE_KB )); then
    echo "Error: Stage2 size is $STAGE2_BIN_SIZE KB, larger than the expected $STAGE2_SIZE KB, udpate configure.sh and make again"
    exit 1
fi
if (( KERNEL_BIN_SIZE > KERNEL_SIZE_KB )); then
    echo "Error: Kernel size is $KERNEL_BIN_SIZE KB, larger than the expected $KERNEL_SIZE KB, udpate configure.sh and make again"
    exit 1
fi

# write raw sectors
$SFS write-sector $IMG $STAGE1_FIRST_SECTOR $STAGE1_SECTOR_COUNT $SRC/stage1.bin
$SFS write-sector $IMG $STAGE2_FIRST_SECTOR $STAGE2_SECTOR_COUNT $SRC/stage2.bin
$SFS write-sector $IMG $KERNEL_FIRST_SECTOR $KERNEL_SECTOR_COUNT $SRC/kernel.bin

# import rootfs
$SFS import-dir $IMG $ROOTFS /

# cross fingers
