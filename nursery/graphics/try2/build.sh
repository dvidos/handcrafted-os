#!/bin/bash
set -e

#     From    (Hex)     To      (Hex)      Size   What
#  -------  -------  -------  -------  --------   ----------------------------
#     0 KB     0x00     1 KB    0x400      1 KB   Interrupt vector, etc
#     1 KB    0x400     2 KB    0x800      1 KB   BIOS data area (kb, timers, etc)
#     2 KB    0x800    31 KB   0x7C00     29 KB   Stage 2 bootloader + its stack
#    31 KB   0x7C00    32 KB   0x8000    512  B   Stage 1 bootloader from BIOS
#    32 KB   0x8000   640 KB  0xA0000    608 KB   Kernel + its stack
#   640 KB  0xA0000     1 MB 0x100000    384 KB   Upper memory / Video / BIOS ROM 
#     1 MB 0x100000    (sky)              (any)   Usable in protected mode

SECTOR_SIZE=512

STAGE1_LOAD_ADDRESS=0x7C00  # at 31KB, this address preset by BIOS
STAGE1_SIZE=$SECTOR_SIZE    # one sector, this size preset by BIOS
STAGE1_STACK_TOP=0x0000     # no stack used on 1st stage

STAGE2_LOAD_ADDRESS=0x0800  # at 2 KB, to clear IVT, BIOS memory area etc.
STAGE2_SIZE_KB=8            # 8 KB currently, can go up to 29 KB
STAGE2_STACK_TOP=0x7C00     # at 31KB, right below stage 1

KERNEL_LOAD_ADDRESS=0x8000  # 32KB, below upper memory (640KB), must be < 1MB, to be loaded in real mode.
KERNEL_SIZE_KB=64           # 64 KB for now, can go up to 608 KB
KERNEL_STACK_TOP=0x90000    # 576KB, below upper memory

STAGE1_SECTORS=$(($STAGE1_SIZE           / $SECTOR_SIZE))
STAGE2_SECTORS=$(($STAGE2_SIZE_KB * 1024 / $SECTOR_SIZE))
KERNEL_SECTORS=$(($KERNEL_SIZE_KB * 1024 / $SECTOR_SIZE))
KERNEL_FIRST_SECTOR_LBA=$(($STAGE1_SECTORS + $STAGE2_SECTORS))

echo "          Address (hex)    Address (dec)          Size      Sectors    Stack top (hex)"
printf "Stage1         %8x    %10d KB     %6d  B       %6d           %8x \n"  $STAGE1_LOAD_ADDRESS $(($STAGE1_LOAD_ADDRESS / 1024)) $STAGE1_SIZE    $STAGE1_SECTORS $STAGE1_STACK_TOP
printf "Stage2         %8x    %10d KB     %6d KB       %6d           %8x \n"  $STAGE2_LOAD_ADDRESS $(($STAGE2_LOAD_ADDRESS / 1024)) $STAGE2_SIZE_KB $STAGE2_SECTORS $STAGE2_STACK_TOP
printf "Kernel         %8x    %10d KB     %6d KB       %6d           %8x \n"  $KERNEL_LOAD_ADDRESS $(($KERNEL_LOAD_ADDRESS / 1024)) $KERNEL_SIZE_KB $KERNEL_SECTORS $KERNEL_STACK_TOP
printf "Kernel first sector LBA: %d\n" $KERNEL_FIRST_SECTOR_LBA

# ------------------------------------------------------------------

pad_file_to_sectors() {
    local file=$1
    local size=$(($2 * $SECTOR_SIZE))
    local cur
    cur=$(stat -c %s "$file")
    (( cur <= size )) || {
        echo "ERROR: $file exceeds limit ($cur > $size)"
        exit 1
    }
    dd if=/dev/zero bs=1 count=$((size - cur)) status=none >> "$file"
}

# ------------------------------------------------------------------

mkdir -p build

# Stage 1
nasm stage1.asm \
    -DSTAGE2_LOAD_ADDRESS=$STAGE2_LOAD_ADDRESS \
    -DSTAGE2_SECTORS=$STAGE2_SECTORS \
    -f bin -o build/stage1.bin

# Stage 2 bootloader (16 sectors, 8KB)
nasm stage2.asm \
    -DSTAGE2_STACK_TOP=$STAGE2_STACK_TOP \
    -DKERNEL_STACK_TOP=$KERNEL_STACK_TOP \
    -DKERNEL_LOAD_ADDRESS=$KERNEL_LOAD_ADDRESS \
    -f elf32 -o build/stage2_asm.o
i686-elf-gcc -m16 -ffreestanding -fno-pie -O2 \
    -DKERNEL_LOAD_ADDRESS=$KERNEL_LOAD_ADDRESS \
    -DKERNEL_FIRST_SECTOR_LBA=$KERNEL_FIRST_SECTOR_LBA \
    -DKERNEL_SECTORS=$KERNEL_SECTORS \
    -c stage2.c -o build/stage2.o
i686-elf-ld -Ttext=$STAGE2_LOAD_ADDRESS -e _stage2_start -o build/stage2.elf build/stage2_asm.o build/stage2.o 
objcopy -O binary build/stage2.elf build/stage2.bin
pad_file_to_sectors build/stage2.bin $STAGE2_SECTORS

# Kernel
nasm kernel.asm -f elf32 -o build/kernel_asm.o
i686-elf-gcc -m32 -ffreestanding -fno-pie -nostdlib -O2 -c kernel.c -o build/kernel.o
i686-elf-ld -m elf_i386 -T kernel.ld \
    --defsym KERNEL_LOAD_ADDRESS=$KERNEL_LOAD_ADDRESS \
    -o build/kernel.elf build/kernel_asm.o build/kernel.o
objcopy -O binary build/kernel.elf build/kernel.bin
pad_file_to_sectors build/kernel.bin $KERNEL_SECTORS

# Create image
cat build/stage1.bin build/stage2.bin build/kernel.bin > build/os.img

# Launch QEMU
#qemu-system-i386 -drive format=raw,file=build/os.img -monitor stdio -d int,cpu_reset,guest_errors -no-reboot -no-shutdown
qemu-system-i386 -drive format=raw,file=build/os.img -serial stdio
