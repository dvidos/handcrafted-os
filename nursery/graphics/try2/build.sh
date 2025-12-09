#!/bin/bash
set -e


mkdir -p build

#     address    (dec)   usage
# 0x0000_7C00    (31K)  first boot loader (512 bytes long)
# 0x0000_1000     (4K)  second boot loader (8KB long)
# 0x0000_9000    (32K)  stack for 1st, 2nd bootloader
# 0x0009_0000  (576KB)  stack of kernel (see kernel.ld)
# 0x0010_0000    (1MB)  kernel (text, bss, data, etc)


# Stage 1
nasm stage1.asm -f bin -o build/stage1.bin


# Stage 2 bootloader (16 sectors, 8KB)
nasm stage2.asm -f elf32 -o build/stage2_asm.o
i686-elf-gcc -m16 -ffreestanding -fno-pie -O2 -c stage2.c -o build/stage2.o
i686-elf-ld -Ttext=0x1000 -e _stage2_start -o build/stage2.elf build/stage2_asm.o build/stage2.o 
objcopy -O binary build/stage2.elf build/stage2.bin
dd if=/dev/zero bs=1 count=$((8192 - $(stat -c %s build/stage2.bin))) >> build/stage2.bin  # pad to two sectors


# Kernel
i686-elf-gcc -m32 -ffreestanding -fno-pie -O2 -c kernel.c -o build/kernel.o
i686-elf-ld -m elf_i386 -T kernel.ld -o build/kernel.elf build/kernel.o
objcopy -O binary build/kernel.elf build/kernel.bin


# Create image
cat build/stage1.bin build/stage2.bin build/kernel.bin > build/os.img


# Launch QEMU
# qemu-system-i386 -drive format=raw,file=build/os.img -monitor stdio -d int,cpu_reset
qemu-system-i386 -drive format=raw,file=build/os.img
