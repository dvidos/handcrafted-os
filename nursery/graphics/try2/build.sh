#!/bin/bash
set -e

# Typical computer memory setup
# -------------------------------------------------
# 0x00000 - 0x003FF	 Interrupt vector table (IVT)
# 0x00400 - 0x04FF	 BIOS data area (keyboard, timers, etc.)
# 0x00500 - 0x9FFFF	 Conventional memory usable by DOS/bootloader
# 0xA0000 - 0xFFFFF	 Video memory / BIOS ROM / memory-mapped I/O
# 
# 
#     From    (Hex)     To      (Hex)      Size   What
#  -------  -------  -------  -------  --------   ----------------------------
#     0 KB     0x00     1 KB    0x400      1 KB   Interrupt vector, etc
#     1 KB    0x400     2 KB    0x800      1 KB   BIOS data area
#     2 KB    0x800    31 KB   0x7C00     29 KB   Stage 2 bootloader + its stack
#    31 KB   0x7C00    32 KB   0x8000    512  B   Stage 1 bootloader from BIOS
#    32 KB   0x8000   640 KB  0xA0000    608 KB   Kernel + its stack
#   640 KB  0xA0000     1 MB 0x100000    384 KB   Upper memory / Video / BIOS ROM 
#     1 MB 0x100000    (sky)              (any)   Usable in protected mode


mkdir -p build

# Stage 1
nasm stage1.asm -f bin -o build/stage1.bin


# Stage 2 bootloader (16 sectors, 8KB)
nasm stage2.asm -f elf32 -o build/stage2_asm.o
i686-elf-gcc -m16 -ffreestanding -fno-pie -O2 -c stage2.c -o build/stage2.o
i686-elf-ld -Ttext=0x800 -e _stage2_start -o build/stage2.elf build/stage2_asm.o build/stage2.o 
objcopy -O binary build/stage2.elf build/stage2.bin
dd if=/dev/zero bs=1 count=$((8192 - $(stat -c %s build/stage2.bin))) >> build/stage2.bin  # pad to 8K size / 16 sectors


# Kernel
nasm kernel.asm -f elf32 -o build/kernel_asm.o
i686-elf-gcc -m32 -ffreestanding -fno-pie -nostdlib -O2 -c kernel.c -o build/kernel.o
i686-elf-ld -m elf_i386 -T kernel.ld -o build/kernel.elf build/kernel_asm.o build/kernel.o
objcopy -O binary build/kernel.elf build/kernel.bin
dd if=/dev/zero bs=1 count=$((65536 - $(stat -c %s build/kernel.bin))) >> build/kernel.bin  # pad to 64K size / 128 sectors

# Create image
cat build/stage1.bin build/stage2.bin build/kernel.bin > build/os.img


# Launch QEMU
#qemu-system-i386 -drive format=raw,file=build/os.img -monitor stdio -d int,cpu_reset,guest_errors -no-reboot -no-shutdown
# qemu-system-i386 -drive format=raw,file=build/os.img -monitor stdio -d guest_errors
qemu-system-i386 -drive format=raw,file=build/os.img -serial stdio
