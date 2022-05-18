#!/bin/sh

# ./iso.sh

qemu-system-i386 \
    -m 2G \
    -cdrom handcrafted-os.iso \
    -chardev stdio,id=char0,logfile=qemu.log,signal=off \
    -serial chardev:char0

