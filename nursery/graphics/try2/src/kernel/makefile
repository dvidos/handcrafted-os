
CYAN := \\033[36m
RESET:= \\033[0m
BOLD := \\033[1m

# caller must define KERNEL_LOAD_ADDRESS, for us to know where to put it.
KERNEL_LOAD_ADDRESS ?= 0xBAD

AS=nasm
CC=i686-elf-gcc
LD=i686-elf-ld

BOOTABLE_CFLAGS=-DARCH=BOOTABLE_i386 -DBARE_METAL_ENV -m32 -ffreestanding -fno-pie -nostdlib -O2 -I.
BOOTABLE_LDFLAGS=-m elf_i386 -T kernel.ld --defsym KERNEL_LOAD_ADDRESS=$(KERNEL_LOAD_ADDRESS)
HOSTED_GCC_FLAGS=-DARCH=HOSTED_x86_64 -DHOSTED_ENV -g \
                -Wno-builtin-declaration-mismatch -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
                -I/usr/include/x86_64-linux-gnu \
                -L/usr/lib/x86_64-linux-gnu \
                $(shell sdl2-config --cflags)
HOSTED_LIBS=$(shell sdl2-config --libs)

COMMON_C_FILES= $(filter-out %.inc.c, \
    kernel.c \
    $(wildcard cpu/*.c) \
    $(wildcard memory/*.c) \
    $(wildcard devices/*.c) \
    $(wildcard graphics/*.c) \
    $(wildcard graphics/gbuffer/*.c) \
    $(wildcard graphics/fonts/*.c) \
    $(wildcard graphics/cursors/*.c) \
    $(wildcard graphics/icons/*.c) \
    $(wildcard concepts/*.c) \
    $(wildcard app_kit/*.c) \
    $(wildcard app_kit/views/*.c) \
)
COMMON_C_OBJS=$(COMMON_C_FILES:.c=.o)

BOOTABLE_ASM_FILES=$(wildcard architecture/bootable_i386/*.asm)
BOOTABLE_ASM_OBJS=$(BOOTABLE_ASM_FILES:.asm=_asm.o)
BOOTABLE_C_FILES=$(wildcard architecture/bootable_i386/*.c)
BOOTABLE_C_OBJS=$(BOOTABLE_C_FILES:.c=.o)

HOSTED_C_FILES = $(wildcard ./architecture/hosted_x86_64/*.c)


# ---------------------

all: kernel.elf hosted_x86_64

kernel.elf: $(BOOTABLE_ASM_OBJS) $(BOOTABLE_C_OBJS) $(COMMON_C_OBJS)
	@echo ---- Making $@ ----
	$(LD) $(BOOTABLE_LDFLAGS) -o $@ $(BOOTABLE_ASM_OBJS) $(BOOTABLE_C_OBJS) $(COMMON_C_OBJS)
	
%_asm.o: %.asm
	$(AS) $< -f elf32 -o $@

%.o: %.c
	$(CC) $(BOOTABLE_CFLAGS) -c $< -o $@



hosted_x86_64: $(HOSTED_C_FILES) $(COMMON_C_FILES)
	@echo ---- $(CYAN) Making $@ $(RESET) ----
	gcc $(HOSTED_GCC_FLAGS) $(HOSTED_C_FILES) $(COMMON_C_FILES) -o $@ $(HOSTED_LIBS)

