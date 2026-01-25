#!/bin/sh

# IMPORTANT: If qemu captures the keyboard and mouse, you can get it back with Ctrl+Alt+G
#            Also, toggle fullscreen with Ctrl+Alt+F



# for qemu arguments, see qemu man page, or 
# https://www.qemu.org/docs/master/system/qemu-manpage.html
# some examples:
#    -drive id=disk,file=my.img,if=none \
#    -device ahci,id=ahci \
#    -device ide-hd,drive=disk,bus=ahci.0 \
#    -cdrom handcrafted-os.iso \
#    -hda linux_disk.img
#
# for more switches also see https://wiki.osdev.org/QEMU
# -no-reboot -no-shutdown   for use in case of tripple fault
# -D <file>                 use log file for debugging info
# -d <something>            what debug information to include
# -m <size>                 memory size to emulate

if [ "$1" = "native" ]; then
    # a and b are for floppies, c,d,e,f are disks 1-4 etc.
    BOOT_ORDER="c"
elif [ "$1" = "grub" ]; then
    # a and b are for floppies, c,d,e,f are disks 1-4 etc.
    BOOT_ORDER="d"
else
    echo "Syntax: emu <boot-img>"
    echo "    <boot-img> can be [ grub | native ]"
    exit 1
fi

DISK_IMG="images/hcos.img"          # contains boot sectors, FAT file system and files
CDROM_IMG="images/hcos-rescue.iso"  # contains boot & kernel only

# DISPLAY_OPTIONS="-display gtk,full-screen=on"
DISPLAY_OPTIONS="-display gtk"

CPU=486

MEMORY=2G

DEBUG_OPTIONS="\
    -D qemu-debug.log \
    -no-reboot -no-shutdown \
    -d cpu_reset,int"

SERIAL_PORT_OPTIONS="\
    -chardev stdio,id=char0,logfile=qemu-serial.log,signal=off  \
    -serial chardev:char0"

DISK_OPTIONS="\
    -drive file=$DISK_IMG,if=ide,index=0,media=disk,format=raw  \
    -drive file=$CDROM_IMG,if=ide,index=2,media=cdrom,format=raw"

BOOT_OPTIONS="-boot order=$BOOT_ORDER"

qemu-system-i386 \
    -m $MEMORY \
    $DISK_OPTIONS \
    $BOOT_OPTIONS  \
    $DISPLAY_OPTIONS \
    $DEBUG_OPTIONS \
    $SERIAL_PORT_OPTIONS

