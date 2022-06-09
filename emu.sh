#!/bin/sh

# ./iso.sh

qemu-system-i386 \
    -m 2G \
    \
    -cdrom handcrafted-os.iso \
    \
    -chardev stdio,id=char0,logfile=qemu.log,signal=off \
    -serial chardev:char0 \
    \
    -drive id=disk,file=my.img,if=none \
    -device ahci,id=ahci \
    -device ide-hd,drive=disk,bus=ahci.0

