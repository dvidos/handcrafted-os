CC = i686-elf-gcc
LD = i686-elf-ld
AS = i686-elf-as
AR = i686-elf-ar

SYSROOT_DIR = ../../sysroot
LIBC_INCLUDES_DIR = ../libc/include
LIBC_BIN_PATH = ../libc
LIBC_BIN = libc.a

CFLAGS = \
	-std=gnu99 \
	-nostdlib \
	-ffreestanding \
	-Wall -Wextra \
	-Werror   \
	-Wno-unused-parameter \
	-O2 \
	-g0 \
	-I$(LIBC_INCLUDES_DIR) \
	-L$(LIBC_BIN_PATH) \
	-l:$(LIBC_BIN)


# we put the binaries in bin, to allow for easy copying to the final image
# we put the dependencies in dep, to allow for easy cleanup
TEMP_DIRS = ./bin ./dep
BIN_FILES = bin/sh   bin/vi   $(patsubst misc/%.c,bin/%,$(wildcard misc/*.c))
DEP_FILES = dep/sh.d dep/vi.d $(patsubst misc/%.c,dep/%.d,$(wildcard misc/*.c))



all: $(TEMP_DIRS) $(BIN_FILES)

$(TEMP_DIRS): makefile
	mkdir -p $(TEMP_DIRS)

bin/sh: $(wildcard sh/*.c)
	$(CC) $^ -M -MF $(patsubst bin/%,dep/%.d,$@) $(CFLAGS)
	$(CC) $^ -o $@ $(CFLAGS)

bin/vi: $(wildcard vi/*.c)
	$(CC) $^ -M -MF $(patsubst bin/%,dep/%.d,$@) $(CFLAGS)
	$(CC) $^ -o $@ $(CFLAGS)

bin/%: misc/%.c
	$(CC) $< -M -MF $(patsubst bin/%,dep/%.d,$@) $(CFLAGS)
	$(CC) $< -o $@ $(CFLAGS)

clean:
	rm -rf $(TEMP_DIRS)



-include $(DEP_FILES)

