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

KERNEL_FILES = $(wildcard kernel/*.c)
KERNEL_HEADERS = $(wildcard kernel/*.h)
KERNEL_OBJS = $(KERNEL_FILES:.c=.o)

.PHONY:


# target to be executed when no arguments to make
default: run

# this was an effort for bios loading the first 512 bytes
# currently it does not load and boot anything
boot/boot_sector.bin: boot/boot_sector.asm
	nasm -f bin $< -o $@

# for info on putting this on an .iso image
# and burning a USB stick, see https://wiki.osdev.org/Bare_Bones
kernel.bin: boot/multiboot.o $(KERNEL_OBJS) linker.ld
	$(CC) -T linker.ld -o $@ -ffreestanding -O2 -nostdlib boot/multiboot.o $(KERNEL_OBJS) -lgcc


boot/multiboot.o: boot/multiboot.s
	$(AS) $< -o $@


kernel/%.o: kernel/%.c $(KERNEL_HEADERS)
	$(CC) -c $< -o $@ -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Wunused-function


view: kernel.bin
	xxd $< | less

run: kernel.bin
	qemu-system-x86_64 -kernel kernel.bin
    
clean:
	rm -f *.o *.bin boot/*.o kernel/*.o



