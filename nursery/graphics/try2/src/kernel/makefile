# caller must define KERNEL_LOAD_ADDRESS, for us to know where to put it.
KERNEL_LOAD_ADDRESS ?= 0xBAD

AS=nasm
CC=i686-elf-gcc
LD=i686-elf-ld
CFLAGS=-m32 -ffreestanding -fno-pie -nostdlib -O2 -I.
LDFLAGS=-m elf_i386 -T kernel.ld --defsym KERNEL_LOAD_ADDRESS=$(KERNEL_LOAD_ADDRESS)

ASM_OBJ=kernel_asm.o
C_OBJS= \
    kernel.o \
    concepts/logger.o \
    cpu/exceptions.o \
    cpu/idt.o \
    cpu/pic.o \
    cpu/timer.o \
    memory/string.o \
    memory/malloc.o \
    memory/sprintf.o \
    devs/serial.o \
    devs/mouse.o \
    devs/keyboard.o \
    graphics/graphics.o \
    graphics/gbuffer.o \
    graphics/color.o \
    graphics/fonts/font8x16.o \
    graphics/fonts/font_mits7.o \
    graphics/fonts/font_geneva9.o \
    graphics/fonts/font_geneva9_bold.o \
    graphics/fonts/font_geneva9_mono.o \
    graphics/cursors/mouse_cursor.o \
    concepts/events.o \
    concepts/screen_manager.o \
    app_kit/surface.o \
    app_kit/graphics_context.o

kernel.elf: $(ASM_OBJ) $(C_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(ASM_OBJ) $(C_OBJS)
	
kernel_asm.o: kernel.asm
	$(AS) $< -f elf32 -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@




