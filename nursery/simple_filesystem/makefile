GCC_OPTIONS = \
	-g \
	-Wall \
	-Wno-unused-function

SRC_FILES = \
	src/simple_filesystem.c \
	dependencies/mem_allocator.c \
	dependencies/sector_device.c \
	dependencies/clock_device.c

all_test: tests/bbtest tests/washtest cli_tool/sfs


tests/bbtest: tests/black_box_tests.c $(SRC_FILES)
	gcc $(GCC_OPTIONS) -o tests/bbtest tests/black_box_tests.c $(SRC_FILES)

tests/washtest: tests/wash_tests.c $(SRC_FILES)
	gcc $(GCC_OPTIONS) -o tests/washtest tests/wash_tests.c $(SRC_FILES)

cli_tool/sfs: cli_tool/sfs.c $(SRC_FILES)
	gcc $(GCC_OPTIONS) -o cli_tool/sfs cli_tool/sfs.c $(SRC_FILES)
