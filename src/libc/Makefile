CC = i686-elf-gcc
LD = i686-elf-ld
AS = i686-elf-as
AR = i686-elf-ar

COMMON_CFLAGS = -std=gnu99 -ffreestanding   \
	-Wall -Wextra -Werror  \
	-O2 -g -Iinclude
LIBC_CFLAGS = $(COMMON_CFLAGS) -D__is_libc
LIBK_CFLAGS = $(COMMON_CFLAGS) -D__is_libk

SOURCE_FILES = \
	$(wildcard *.c) \
	$(wildcard arch/*.c) \
	$(wildcard stdio/*.c) \
	$(wildcard stdlib/*.c) \
	$(wildcard proc/*.c) \
	$(wildcard string/*.c) \
	$(wildcard sys/*.c)

LIBC_OBJ_FILES = $(SOURCE_FILES:.c=.o)
LIBK_OBJ_FILES = $(SOURCE_FILES:.c=.libk.o)

HEADER_FILES = \
	$(wildcard include/*.h)

SYSROOT_DIR = ../../sysroot

.SUFFIXES: .o .libk.o .c



all: libc.a libk.a install

libc.a: $(LIBC_OBJ_FILES)
	$(AR) rcs $@ $(LIBC_OBJ_FILES)

libk.a: $(LIBK_OBJ_FILES)
	$(AR) rcs $@ $(LIBK_OBJ_FILES)

.c.o:
	$(CC) -c $< -o $@ $(LIBC_CFLAGS)

.c.libk.o:
	$(CC) -c $< -o $@ $(LIBK_CFLAGS)

clean:
	rm -f libc.a libk.a *.a
	rm -f $(LIBC_OBJ_FILES) $(LIBK_OBJ_FILES) *.o */*.o */*/*.o
	rm -f $(LIBC_OBJ_FILES:.o=.d) $(LIBK_OBJ_FILES:.o=.d) *.d */*.d */*/*.d

install: install-headers install-libs
	
install-headers:
	mkdir -p $(SYSROOT_DIR)/usr/include
	cp -R --preserve=timestamps include/. $(SYSROOT_DIR)/usr/include

install-libs: libc.a libk.a
	mkdir -p $(SYSROOT_DIR)/usr/lib
	cp -R --preserve=timestamps libc.a libk.a $(SYSROOT_DIR)/usr/lib

