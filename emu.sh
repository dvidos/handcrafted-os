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

# for more switches also see https://wiki.osdev.org/QEMU
# -no-reboot -no-shutdown   for use in case of tripple fault
# -D <file>                 use log file for debugging info
# -d <something>            what debug information to include
# -m <size>                 memory size to emulate


qemu-system-i386 \
    -display gtk,full-screen=on \
    -D ./qemu.log \
    -no-reboot -no-shutdown -d cpu_reset,int \
    -m 2G \
    \
    -chardev stdio,id=char0,logfile=qemu-serial.log,signal=off \
    -serial chardev:char0 \
    \
    -drive file=hcos.img,if=ide,index=0,media=disk,format=raw    \
    -drive file=handcrafted-os.iso,if=ide,index=2,media=cdrom    \
    \
    -drive id=disk1,file=imgs/linux_disk.img,if=none \
    -device ahci,id=ahci \
    -device ide-hd,drive=disk1,bus=ahci.0 \
    \
    -boot order=d


