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
CFLAGS = -std=gnu99 -ffreestanding -O3 \
		-Icore/include  \
		-Wall \
		-Wextra  \
		-Werror   \
		-Wno-unused-parameter \
		-Wno-unused-function \
		-Wno-unused-variable \
		-Wno-unused-but-set-variable \
		-Wno-array-bounds \
		-Wno-pointer-sign

KERNEL_FILES = \
	$(wildcard core/*.c) \
	$(wildcard core/memory/*.c) \
	$(wildcard core/drivers/*.c) \
	$(wildcard core/multitask/*.c) \
	$(wildcard core/konsole/*.c) \
	$(wildcard core/devices/*.c) \
	$(wildcard core/filesys/*.c) \
	$(wildcard core/filesys/fat/*.c) \
	$(wildcard core/klib/*.c)


KERNEL_C_INCLUDES = \
	$(wildcard core/filesys/fat/*.cincl)

KERNEL_HEADERS = \
	$(wildcard core/*.h) \
	$(wildcard core/include/*.h) \
	$(wildcard core/boot/*.h) \
	$(wildcard core/memory/*.h) \
	$(wildcard core/drivers/*.h) \
	$(wildcard core/multitask/*.h) \
	$(wildcard core/konsole/*.h) \
	$(wildcard core/devices/*.h) \
	$(wildcard core/filesys/*.h) \
	$(wildcard core/filesys/fat*.h) \
	$(wildcard core/klib/*.h)


KERNEL_ASM_FILES = \
	$(wildcard core/*.asm) \
	$(wildcard core/boot/*.asm) \
	$(wildcard core/multitask/*.asm)

SYSROOT_DIR = ../../sysroot
KERNEL_LIB = $(SYSROOT_DIR)/usr/lib/libk.a

KERNEL_OBJS = $(KERNEL_FILES:.c=.o)
KERNEL_ASM_OBJS = $(KERNEL_ASM_FILES:.asm=.o)

.PHONY:


# target to be executed when no arguments to make
default: kernel.bin install

# this was an effort for bios loading the first 512 bytes
# currently it does not load and boot anything
boot/boot_sector.bin: boot/boot_sector.asm
	nasm -f bin $< -o $@

# for info on putting this on an .iso image
# and burning a USB stick, see https://wiki.osdev.org/Bare_Bones
kernel.bin: boot/multiboot.o $(KERNEL_OBJS) $(KERNEL_ASM_OBJS) $(KERNEL_LIB) linker.ld 
	$(CC) -T linker.ld -o $@ -ffreestanding -O2 -nostdlib boot/multiboot.o $(KERNEL_OBJS) $(KERNEL_ASM_OBJS) $(KERNEL_LIB) -lgcc


boot/multiboot.o: boot/multiboot.S
	$(AS) $< -o $@


core/%.o: core/%.c $(KERNEL_HEADERS) Makefile
	$(CC) -c $< -o $@ $(CFLAGS)


core/%.o: core/%.asm
	nasm -f elf $< -o $@

core/%.o: core/%.S
	$(AS) $< -o $@

clean:
	rm -f *.o *.bin boot/*.o core/*.o

install:
	mkdir -p $(SYSROOT_DIR)/boot
	cp --preserve=timestamps kernel.bin $(SYSROOT_DIR)/boot
