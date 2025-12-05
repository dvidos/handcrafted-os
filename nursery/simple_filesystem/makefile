GCC_OPTIONS = \
	-g \
	-Wall \
	-Wno-unused-function

SRC_FILES = \
	src/simple_filesystem.c \
	dependencies/mem_allocator.c \
	dependencies/sector_device.c \
	dependencies/clock_device.c

all_test: bbtest washtest


bbtest: black_box_tests.c $(SRC_FILES)
	gcc $(GCC_OPTIONS) -o bbtest black_box_tests.c $(SRC_FILES)

washtest: wash_tests.c $(SRC_FILES)
	gcc $(GCC_OPTIONS) -o washtest wash_tests.c $(SRC_FILES)