#!/bin/bash

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
