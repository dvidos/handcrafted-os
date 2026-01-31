#!/bin/sh

# based on all created artifacts inside ./build, create the OS image
# we should have various addresses and sizes passed into us?
# use the ./sfs/sfs cli tool to generate the image

. ./config.sh  # get build variables

IMG=os.img
SFS=./tools/sfs/sfs1
SRC=build/kernel

echo "Making file $IMG with size $DISK_IMAGE_SIZE_MB MB"

# create
$SFS create -i $IMG --size ${DISK_IMAGE_SIZE_MB}M

# copy sectors
$SFS wrsect -i $IMG -s $STAGE1_FIRST_SECTOR -c $STAGE1_SECTOR_COUNT -f $SRC/stage1.bin
$SFS wrsect -i $IMG -s $STAGE2_FIRST_SECTOR -c $STAGE2_SECTOR_COUNT -f $SRC/stage2.bin
$SFS wrsect -i $IMG -s $KERNEL_FIRST_SECTOR -c $KERNEL_SECTOR_COUNT -f $SRC/kernel.bin

# create partition
$SFS wrpart -i $IMG --entry 1 \
    --first-sector $PARTITION_1_FIRST_SECTOR \
    --sector-count $PARTITION_1_SECTOR_COUNT \
    --type 0x7F --bootable 1

# create FS
$SFS mkfs -i $IMG --start-sector $PARTITION_1_FIRST_SECTOR --label HANDCRAFTED-OS

# copy rootfs


# print diagnostics
