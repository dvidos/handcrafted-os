#!/bin/sh

# ./iso.sh

# for qemu arguments, see man page, or 
# https://www.qemu.org/docs/master/system/qemu-manpage.html
# some examples:
#    -drive id=disk,file=my.img,if=none \
#    -device ahci,id=ahci \
#    -device ide-hd,drive=disk,bus=ahci.0 \
#    -cdrom handcrafted-os.iso \
#    -hda linux_disk.img

qemu-system-i386 \
    -m 2G \
    \
    -chardev stdio,id=char0,logfile=qemu.log,signal=off \
    -serial chardev:char0 \
    \
    -drive id=disk1,file=linux_disk.img,if=none \
    -device ahci,id=ahci \
    -device ide-hd,drive=disk1,bus=ahci.0 \
    \
    -drive file=dos_disk.img,if=ide,index=0,media=disk,format=raw  \
    -drive file=handcrafted-os.iso,if=ide,index=2,media=cdrom    \
    \
    -boot order=d
