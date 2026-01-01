# caller must define KERNEL_LOAD_ADDRESS, for us to know where to put it.
KERNEL_LOAD_ADDRESS ?= 0xBAD

AS=nasm
CC=i686-elf-gcc
LD=i686-elf-ld
CFLAGS=-m32 -ffreestanding -fno-pie -nostdlib -O2 -I.
LDFLAGS=-m elf_i386 -T kernel.ld --defsym KERNEL_LOAD_ADDRESS=$(KERNEL_LOAD_ADDRESS)

ASM_OBJ=kernel_asm.o
C_SOURCES= \
    kernel.c \
    $(wildcard cpu/*.c) \
    $(wildcard memory/*.c) \
    $(wildcard devices/*.c) \
    $(wildcard concepts/*.c) \
    $(wildcard graphics/*.c) \
    $(wildcard graphics/fonts/*.c) \
    $(wildcard graphics/cursors/*.c) \
    $(wildcard graphics/icons/*.c) \
    $(wildcard app_kit/*.c) \

C_SOURCES_WITHOUT_INCLUDES=$(filter-out %.inc.c,$(C_SOURCES))
C_OBJS=$(C_SOURCES_WITHOUT_INCLUDES:.c=.o)

kernel.elf: $(ASM_OBJ) $(C_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(ASM_OBJ) $(C_OBJS)
	
kernel_asm.o: kernel.asm
	$(AS) $< -f elf32 -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@




