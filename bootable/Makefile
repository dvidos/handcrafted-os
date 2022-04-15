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



.PHONY:


# target to be executed when no arguments to make
default: run

# this was an effort for bios loading the first 512 bytes
# currently it does not load and boot anything
boot/boot_sector.bin: boot/boot_sector.asm
	nasm -f bin $< -o $@

# for info on putting this on an .iso image
# and burning a USB stick, see https://wiki.osdev.org/Bare_Bones
kernel.bin: boot/multiboot.o kernel/kernel.o linker.ld
	$(CC) -T linker.ld -o $@ -ffreestanding -O2 -nostdlib boot/multiboot.o kernel/kernel.o -lgcc

boot/multiboot.o: boot/multiboot.s
	$(AS) $< -o $@

kernel/kernel.o: kernel/kernel.c
	@#$(CC) -ffreestanding -c $< -o $@
	$(CC) -c $< -o $@ -std=gnu99 -ffreestanding -O2 -Wall -Wextra

view: kernel.bin
	xxd $< | less

run: kernel.bin
	# simple format was: qemu-system-x86_64 $?
	@#qemu-system-x86_64 -drive file=$?,format=raw,index=0,media=disk 
	qemu-system-x86_64 -kernel kernel.bin
    
clean:
	rm -f *.o *.bin



