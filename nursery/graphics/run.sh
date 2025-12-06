#!/bin/bash
set -e

# Compile
gcc -m32 -ffreestanding -fno-pie -c kernel.c -o kernel.o
gcc -m32 -ffreestanding -fno-pie -c multiboot_header.c -o multiboot_header.o

# Link as ELF
ld -m elf_i386 -T link.ld -o kernel.elf multiboot_header.o kernel.o

# Prepare ISO structure
mkdir -p iso/boot/grub
cp kernel.elf iso/boot/           # note: kernel.elf, not kernel.bin
cp grub.cfg iso/boot/grub/

# Build ISO
grub-mkrescue -o graphics.iso iso

# Run in QEMU
qemu-system-i386 -cdrom graphics.iso -vga std
