C_OPTIONS = -Wall -Wno-unused-function
SRC_FILES = \
	tests.c \
	src/simple_filesystem.c \
	dependencies/mem_allocator.c \
	dependencies/sector_device.c 

TESTS:
	gcc -g -o tests $(C_OPTIONS) $(SRC_FILES)