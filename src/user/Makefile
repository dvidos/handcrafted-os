CC = i686-elf-gcc
LD = i686-elf-ld
AS = i686-elf-as
AR = i686-elf-ar

DISKIMG_DIR = ../../diskimg
OUR_INCLUDES_DIR = $(DISKIMG_DIR)/usr/include
OUR_HEADER_FILES = $(wildcard $(OUR_INCLUDES_DIR)/*)
OUR_LIBC = $(DISKIMG_DIR)/usr/lib/libc.a

CFLAGS = \
	-std=gnu99 \
	-nostdlib \
	-ffreestanding \
	-Wall -Wextra \
	-O2 \
	-g \
	-I$(OUR_INCLUDES_DIR) \
	$(OUR_LIBC)

UTILS_SOURCE_FILES = $(wildcard utils/*.c)
UTILS_OUT_FILES = $(UTILS_SOURCE_FILES:.c=)
UTILS_BIN_FILES = $(UTILS_SOURCE_FILES:utils=bin:.o=)

.SUFFIXES:

all: bin/echo install

bin/echo: utils/echo.c $(OUR_HEADER_FILES) $(OUR_LIBC)
	mkdir -p ./bin
	$(CC) $< -o $@ $(CFLAGS)

clean:
	rm -rf ./bin

install:
	mkdir -p $(DISKIMG_DIR)/bin
	cp -R --preserve=timestamps bin/. $(DISKIMG_DIR)/bin
	
