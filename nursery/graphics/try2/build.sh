#!/bin/bash
set -e

mkdir -p build

# Stage 1
nasm stage1.asm -f bin -o build/stage1.bin

# Stage 2 bootloader
nasm stage2.asm -f elf32 -o build/stage2_asm.o
i686-elf-gcc -m16 -ffreestanding -fno-pie -O2 -c stage2.c -o build/stage2.o
i686-elf-ld -Ttext=0x2000 -e _stage2_start -o build/stage2.elf build/stage2.o build/stage2_asm.o
objcopy -O binary build/stage2.elf build/stage2.bin

# Kernel
i686-elf-gcc -m32 -ffreestanding -fno-pie -O2 -c kernel.c -o build/kernel.o
i686-elf-ld -m elf_i386 -T kernel.ld -o build/kernel.elf build/kernel.o
objcopy -O binary build/kernel.elf build/kernel.bin

# Create image
cat build/stage1.bin build/stage2.bin build/kernel.bin > build/os.img

# Launch QEMU
qemu-system-i386 -drive format=raw,file=build/os.img
