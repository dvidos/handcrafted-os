# caller must define KERNEL_LOAD_ADDRESS, for us to know where to put it.
KERNEL_LOAD_ADDRESS ?= 0x12345

AS=nasm
CC=i686-elf-gcc
LD=i686-elf-ld
CFLAGS=-m32 -ffreestanding -fno-pie -nostdlib -O2 -I.
LDFLAGS=-m elf_i386 -T kernel.ld --defsym KERNEL_LOAD_ADDRESS=$(KERNEL_LOAD_ADDRESS)

C_OBJS= \
    kernel.o \
    memory/string.o \
    memory/malloc.o \
    graphics/graphics.o \
    graphics/color.o \
    graphics/font5x9.o

ASM_OBJ=kernel_asm.o

kernel.elf: $(ASM_OBJ) $(C_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(ASM_OBJ) $(C_OBJS)
	
kernel_asm.o: kernel.asm
	$(AS) $< -f elf32 -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@




