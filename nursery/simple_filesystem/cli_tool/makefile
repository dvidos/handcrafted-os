SRCS = sfs.c file_sector_device.c partition_sector_device.c utils.c command_parser.c ../src/simple_filesystem.c ../dependencies/mem_allocator.c ../dependencies/clock_device.c

sfs: $(SRCS)
	gcc -o $@ $^ -I. -I../ -g

clean:
	rm -f sfs
