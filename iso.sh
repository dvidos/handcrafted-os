#!/bin/sh

set -e

# ./build.sh

# for documentation on how to setup the iso image and boot the kernel using grub, 
# see https://wiki.osdev.org/Bare_Bones#Building_a_bootable_cdrom_image

# to put the image in a usb stick, do the following:
# sudo dd if=handcrafted-os.iso of=/dev/sda && sync

mkdir -p tempdir/boot/grub
cp sysroot/boot/kernel.bin tempdir/boot
cp -r sysroot/* tempdir
cat > tempdir/boot/grub/grub.cfg << EOF
menuentry "kernel" {
    multiboot /boot/kernel.bin
}
menuentry "kernel console" {
    multiboot /boot/kernel.bin console
}
menuentry "kernel tests" {
    multiboot /boot/kernel.bin tests
}
EOF
grub-mkrescue -o handcrafted-os.iso tempdir


