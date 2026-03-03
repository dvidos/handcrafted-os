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
SOURCE_FILES = \
	$(wildcard src/*.c) \
	$(wildcard src/arch/*.c) \
	$(wildcard src/stdio/*.c) \
	$(wildcard src/stdlib/*.c) \
	$(wildcard src/proc/*.c) \
	$(wildcard src/string/*.c) \
	$(wildcard src/readline/*.c) \
	$(wildcard src/metrics/*.c) \
	$(wildcard src/sys/*.c) \
	$(wildcard src/data_structs/try_2/*.c)

OBJ_FILES = $(SOURCE_FILES:.c=.o)
DEP_FILES = $(OBJ_FILES:.o=.d)

HEADER_FILES = $(wildcard include/*.h)


all: $(LIBC)

$(LIBC): $(OBJ_FILES)
	$(AR) rcs $@ $(OBJ_FILES)

.c.o:
	$(CC) -MD -c $< -o $@ $(CFLAGS)

tests: $(LIBC)
	# compile and run tests, against the library

clean:
	rm -f $(LIBC) $(OBJ_FILES) $(LIBC_OBJ_DEPS)

# include depenedencies files, produced by "gcc -MD"
-include $(DEP_FILES)
