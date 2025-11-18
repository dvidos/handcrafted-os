C_OPTIONS = -Wall -Wno-unused-function
SRC_FILES = \
	tests.c \
	src/simplest_filesystem.c \
	src/cache.c \
	dependencies/mem_allocator.c \
	dependencies/sector_device.c 

TESTS:
	gcc -g -o tests $(C_OPTIONS) $(SRC_FILES)