# Compiler & linker
CC = gcc
LD = ld
CFLAGS = -m32 -ffreestanding -fno-pie -Wall -Wextra -O2
LDFLAGS = -m elf_i386 -T link.ld

# Source files
SRCS = kernel.c multiboot_header.c vga.c serial.c
OBJS = $(SRCS:.c=.o)

# Targets
KERNEL_ELF = kernel.elf
ISO_DIR = iso
ISO_FILE = graphics.iso

.PHONY: all clean iso iso-build run

# Default target: build ELF
all: $(KERNEL_ELF)

start.o: start.s
	as --32 $< -o $@

# Compile each .c into .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link object files into kernel ELF
$(KERNEL_ELF): start.o $(OBJS)
	$(LD) $(LDFLAGS) -o $@ start.o $(OBJS)

# Clean build artifacts
clean:
	rm -f start.o $(OBJS) $(KERNEL_ELF) $(ISO_FILE)
	rm -rf $(ISO_DIR)

# Prepare ISO structure
iso: $(KERNEL_ELF)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/
	cp grub.cfg $(ISO_DIR)/boot/grub/

# Build ISO using grub-mkrescue
iso-build: iso
	grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)

# Run in QEMU
run: iso-build
	qemu-system-i386 -cdrom $(ISO_FILE) -vga std -serial stdio
	
