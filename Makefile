# Common macros
#   $@ is the target for multiple targets (separated by space)
#   $? are the dependencies newer than the target
#   $^ are all the dependences
#
# For implicit rules (e.g. ".c.o:")
#   $< is the target that caused the action
#   $* is the plain filename, common to both extensions

# using customly built cross compiler on path
CC = i686-elf-gcc
LD = i686-elf-ld
AS = i686-elf-as
EMU = qemu-system-i386 -m 2G
CFLAGS = -std=gnu99 -ffreestanding -O3 -Wall -Wextra  \
		-Werror   \
		-Wno-unused-parameter \
		-Wno-unused-function \
		-Wno-unused-variable \
		-Wno-unused-but-set-variable \
		-Wno-array-bounds

KERNEL_FILES = $(wildcard kernel/*.c)
KERNEL_HEADERS = $(wildcard kernel/*.h)
KERNEL_OBJS = $(KERNEL_FILES:.c=.o)
KERNEL_ASM_FILES = $(wildcard kernel/*.asm)
KERNEL_ASM_OBJS = $(KERNEL_ASM_FILES:.asm=.o)

QEMU_SERIAL_FLAGS = -chardev stdio,id=char0,logfile=qemu.log,signal=off  -serial chardev:char0

.PHONY:


# target to be executed when no arguments to make
default: run

# this was an effort for bios loading the first 512 bytes
# currently it does not load and boot anything
boot/boot_sector.bin: boot/boot_sector.asm
	nasm -f bin $< -o $@

# for info on putting this on an .iso image
# and burning a USB stick, see https://wiki.osdev.org/Bare_Bones
kernel.bin: boot/multiboot.o $(KERNEL_OBJS) $(KERNEL_ASM_OBJS) linker.ld
	$(CC) -T linker.ld -o $@ -ffreestanding -O2 -nostdlib boot/multiboot.o $(KERNEL_OBJS) $(KERNEL_ASM_OBJS) -lgcc


boot/multiboot.o: boot/multiboot.S
	$(AS) $< -o $@


kernel/%.o: kernel/%.c $(KERNEL_HEADERS) Makefile
	@# -O0 disables optimizations (for loops etc), default was -O2
	$(CC) -c $< -o $@ $(CFLAGS)


kernel/%.o: kernel/%.asm
	nasm -f elf $< -o $@

kernel/%.o: kernel/%.S
	$(AS) $< -o $@

view: kernel.bin
	xxd $< | less

# the brug + iso image stuff is from the Barebones article
# https://wiki.osdev.org/Bare_Bones
# can be copied to a usb stick, _very carefully_ by doing:
# "sudo dd if=kernel.iso of=/dev/sdx && sync" -- replace "x" with appropriate letter
# can be mounted for inspection (RO) by doing:
# sudo mount ./kernel.iso /mnt/p1 -o loop
kernel.iso: kernel.bin
	cp kernel.bin iso/boot
	grub-mkrescue -o $@ iso


run: kernel.iso
	@# qemu-system-x86_64 -kernel kernel.bin
	@#$(EMU) -kernel kernel.bin
	$(EMU) -cdrom kernel.iso $(QEMU_SERIAL_FLAGS)


clean:
	rm -f *.o *.bin boot/*.o kernel/*.o


