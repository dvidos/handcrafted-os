CC = i686-elf-gcc
LD = i686-elf-ld
AS = i686-elf-as
AR = i686-elf-ar

SYSROOT_DIR = ../../sysroot
OUR_INCLUDES_DIR = $(SYSROOT_DIR)/usr/include
OUR_HEADER_FILES = $(wildcard $(OUR_INCLUDES_DIR)/*)
OUR_LIBC = $(SYSROOT_DIR)/usr/lib/libc.a

CFLAGS = \
	-std=gnu99 \
	-nostdlib \
	-ffreestanding \
	-Wall -Wextra \
	-Werror   \
	-O2 \
	-g0 \
	-I$(OUR_INCLUDES_DIR) \
	$(OUR_LIBC)



UTILS_SRC_FILES = $(wildcard utils/*.c)
UTILS_DEP_FILES = $(patsubst utils/%.c,bin/%.d,$(UTILS_SRC_FILES))
UTILS_BIN_FILES = $(patsubst utils/%.c,bin/%,$(UTILS_SRC_FILES))
UTILS_BIN_FILES += bin/sh
# UTILS_BIN_FILES += bin/vi

INSTALLED_BINS = $(patsubst utils/%.c,$(SYSROOT_DIR)/%,$(UTILS_SRC_FILES))


all: $(UTILS_BIN_FILES) install

print:
	echo src = $(UTILS_SRC_FILES)
	echo dep = $(UTILS_DEP_FILES)
	echo bin = $(UTILS_BIN_FILES)


-include $(UTILS_DEP_FILES)

bin/sh: $(wildcard utils/sh/*.c)
	mkdir -p ./bin
	$(CC) -MD $^ -o $@ $(CFLAGS)

bin/vi: $(wildcard utils/vi/*.c)
	mkdir -p ./bin
	$(CC) -MD $^ -o $@ $(CFLAGS)

bin/%: utils/%.c
	mkdir -p ./bin
	$(CC) -MD $< -o $@ $(CFLAGS)

install:
	mkdir -p $(SYSROOT_DIR)/bin
	cp -R --preserve=timestamps $(UTILS_BIN_FILES) $(SYSROOT_DIR)/bin

clean:
	rm -rf ./bin
	rm -rf $(UTILS_BIN_FILES) $(UTILS_DEP_FILES)

