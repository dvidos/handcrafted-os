CC = i686-elf-gcc
LD = i686-elf-ld
AS = i686-elf-as
AR = i686-elf-ar

COMMON_CFLAGS = -std=gnu99 -ffreestanding   \
	-Wall -Wextra -Werror  \
	-Wno-unused-parameter \
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
	$(wildcard readline/*.c) \
	$(wildcard metrics/*.c) \
	$(wildcard sys/*.c) \
	$(wildcard data_structs/try_2/*.c)

LIBC_OBJ_FILES = $(SOURCE_FILES:.c=.o)
LIBK_OBJ_FILES = $(SOURCE_FILES:.c=.libk.o)
KERNEL_OBJ_DEPS = $(LIBC_OBJ_FILES:.o=.d)


HEADER_FILES = \
	$(wildcard include/*.h)

SYSROOT_DIR = ../../sysroot

.SUFFIXES: .o .libk.o .c



all: libc.a libk.a

print:
	@echo ---- object files -----
	@echo $(LIBC_OBJ_FILES)


libc.a: $(LIBC_OBJ_FILES)
	$(AR) rcs $@ $(LIBC_OBJ_FILES)

libk.a: $(LIBK_OBJ_FILES)
	$(AR) rcs $@ $(LIBK_OBJ_FILES)

.c.o:
	$(CC) -MD -c $< -o $@ $(LIBC_CFLAGS)

.c.libk.o:
	$(CC) -c $< -o $@ $(LIBK_CFLAGS)


// .PHONY: test
test: tests/run_tests

tests/run_tests: tests/run_tests.c $(SOURCE_FILES)
	gcc -o tests/run_tests \
		-m32 \
		-I./include \
		-D__is_libc  -D__is_tests \
		tests/run_tests.c $(SOURCE_FILES) \
		-Wno-builtin-declaration-mismatch
	./tests/run_tests

clean:
	rm -f libc.a libk.a *.a
	rm -f $(LIBC_OBJ_FILES) $(LIBK_OBJ_FILES) *.o */*.o */*/*.o
	rm -f $(LIBC_OBJ_FILES:.o=.d) $(LIBK_OBJ_FILES:.o=.d) *.d */*.d */*/*.d
	rm -f $(LIBC_OBJ_DEPS)
	rm -f tests/run_tests


# include depenedencies files, produced by "gcc -MD"
-include $(LIBC_OBJ_DEPS)
