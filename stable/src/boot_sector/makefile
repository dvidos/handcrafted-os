
BINARY = boot_sector.bin


$(BINARY): boot_sector.asm makefile
	nasm -f bin $< -o $@

clean:
	rm -f *.bin

# not exactly 1:1, but we are getting there.
dissassembly:
	objdump -b binary -EL -m i8086 -M i8086 -M intel --adjust-vma=0x7c00 -D $(BINARY)

# how to see the resulting binary, compare to dissassembled hex
hexdump:
	hexdump -C $(BINARY)


