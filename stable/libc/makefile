include ../config.inc.mk
ifeq ($(strip $(BUILD_DIR)),)
    $(error BUILD_DIR is empty. Check your config.inc.mk)
endif


# pulls includes from both places (e.g. "#include <kernel/syscall.h>")
CFLAGS = -std=gnu99 -ffreestanding   \
	-nostdlib -nostdinc -fno-builtin \
	-Wall -Wextra -Werror  \
	-Wno-unused-parameter \
	-O2 -g \
	-I./include -I$(BUILD_DIR)/rootfs/usr/include

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
	$(wildcard src/sys/ioctl/*.c) \
	$(wildcard src/time/*.c) \
	$(wildcard src/unistd/*.c) \
	$(wildcard src/utime/*.c)

ASM_FILES = $(wildcard src/hcos/*.asm)

OBJ_FILES = $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o) 
DEP_FILES = $(C_FILES:.c=.d)


all: $(LIBC)

$(LIBC): $(OBJ_FILES)
	$(AR) rcs $@ $(OBJ_FILES)

%.o: %.c
	$(CROSS_CC) -MD -c $< -o $@ $(CFLAGS)

%.o: %.asm
	nasm -f elf $< -o $@

.PHONY: install
install: $(LIBC)
	mkdir -p $(BUILD_DIR)/rootfs/usr/lib
	cp -a $(LIBC) -t $(BUILD_DIR)/rootfs/usr/lib

.PHONY: install_headers
install_headers:
	mkdir -p $(BUILD_DIR)/rootfs/usr/include
	cp -ar include/* -t $(BUILD_DIR)/rootfs/usr/include

tests: $(LIBC)
	# compile and run tests, against the library

clean:
	rm -f $(LIBC) $(OBJ_FILES) $(LIBC_OBJ_DEPS)

# include depenedencies files, produced by "gcc -MD"
-include $(DEP_FILES)
