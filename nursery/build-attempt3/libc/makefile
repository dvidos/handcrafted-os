include ../config.mk

CC = i686-elf-gcc
LD = i686-elf-ld
AS = i686-elf-as
AR = i686-elf-ar

CFLAGS = -std=gnu99 -ffreestanding   \
	-nostdlib -nostdinc -fno-builtin \
	-Wall -Wextra -Werror  \
	-Wno-unused-parameter \
	-O2 -g -I./include

LIBC=libc.a
C_FILES = \
	$(wildcard src/*.c) \
	$(wildcard src/assert/*.c) \
	$(wildcard src/ctype/*.c) \
	$(wildcard src/dirent/*.c) \
	$(wildcard src/errno/*.c) \
	$(wildcard src/fcntl/*.c) \
	$(wildcard src/hcos/*.c) \
	$(wildcard src/math/*.c) \
	$(wildcard src/setjmp/*.c) \
	$(wildcard src/signal/*.c) \
	$(wildcard src/stdio/*.c) \
	$(wildcard src/stdlib/*.c) \
	$(wildcard src/string/*.c) \
	$(wildcard src/sys/*.c) \
	$(wildcard src/sys/stat/*.c) \
	$(wildcard src/sys/wait/*.c) \
	$(wildcard src/time/*.c) \
	$(wildcard src/unistd/*.c) \
	$(wildcard src/utime/*.c)

ASM_FILES = $(wildcard src/hcos/*.asm)

OBJ_FILES = $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o) 
DEP_FILES = $(C_FILES:.c=.d)

HEADER_FILES = \
	$(wildcard include/*.h) \
	$(wildcard include/sys/*.h) \
	$(wildcard include/hcos/*.h)

UAPI_SRC_DIR = ../kernel/include/uapi
UAPI_DEST_DIR = ./include/kernel
UAPI_SRC_HEADERS = $(wildcard $(UAPI_SRC_DIR)/*.h)
UAPI_DEST_HEADERS = $(patsubst $(UAPI_SRC_DIR)/%,$(UAPI_DEST_DIR)/%,$(UAPI_SRC_HEADERS))




all: $(LIBC)

$(LIBC): $(UAPI_DEST_HEADERS) $(OBJ_FILES)
	$(AR) rcs $@ $(OBJ_FILES)

%.o: %.c
	$(CC) -MD -c $< -o $@ $(CFLAGS)

%.o: %.asm
	nasm -f elf $< -o $@

$(UAPI_DEST_DIR)/%.h: $(UAPI_SRC_DIR)/%.h
	@mkdir -p $(@D)
	cp $< $@

tests: $(LIBC)
	# compile and run tests, against the library

clean:
	rm -f $(LIBC) $(OBJ_FILES) $(LIBC_OBJ_DEPS) $(UAPI_DEST_HEADERS)
	rmdir $(UAPI_DEST_DIR)

# include depenedencies files, produced by "gcc -MD"
-include $(DEP_FILES)
