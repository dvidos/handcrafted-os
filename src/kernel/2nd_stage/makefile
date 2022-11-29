CC = i686-elf-gcc
AS = i686-elf-as
CFLAGS = -std=gnu99 -ffreestanding -O0

BINARY = boot_loader2.bin
OBJECTS = start.o main.o

# the idea is to put the asm startup code, as well as the C code
# all into one executable that will be loaded (by the boot sector)
# the desired mapping is: 0x10000 for code, then data following along.
# the problem is that we need the `_start` method to be exactly at the start of the loaded address
# 
# this link maybe has some interesting ideas
# https://www.itcodar.com/linux/how-to-get-gcc-to-output-raw-binary.html

# can see the resulting boot loader using:


$(BINARY): $(OBJECTS) makefile linker.ld
	$(CC) -T linker.ld -o $@ -ffreestanding -O0 -nostdlib $(OBJECTS)

start.o: start.asm
	nasm -f elf $< -o $@

main.o: main.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f *.o *.bin *.l

# how to dissassemble something
dissassembly:
	objdump -b binary -EL -m i8086 -M i8086 -M intel --adjust-vma=0x10000 -D $(BINARY)

# how to see the resulting binary, compare to dissassembled hex
hexdump:
	hexdump -C $(BINARY)


