#!/bin/sh

set -e  # stop on first failure


echo "Building first stage..."
cd src/kernel/1st_stage
make
cd ../../..

echo "Building second stage..."
cd src/kernel/2nd_stage
make
cd ../../..

echo "Making image..."
S1="src/kernel/1st_stage/boot_sector.bin"
S2="src/kernel/2nd_stage/boot_loader2.bin"
ls -l $S1
ls -l $S2
cat $S1 $S2 > bootable.img



qemu-system-i386 -hda bootable.img

