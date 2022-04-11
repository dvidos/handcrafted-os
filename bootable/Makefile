# Common macros
#   $@ is the target for multiple targets (separated by space)
#   $? are the dependencies newer than the target
#   $^ are all the dependences
#
# For implicit rules (e.g. ".c.o:")
#   $< is the target that caused the action
#   $* is the plain filename, common to both extensions

.PHONY:


# target to be executed when no arguments to make
default: run




boot_sector.bin: boot_sector.asm
	nasm -f bin $< -o $@

kernel32.bin: kernel_entry32.o kernel32.o
	@# kernel has special entrance in assembly
	@# kernel load offset has to be updated in boot_sector as well
	@# emulations supported can be 
	ld -o $@ -Ttext 0x1000 $^ --oformat i386

kernel64.bin: kernel_entry64.o kernel64.o
	@# kernel has special entrance in assembly
	@# kernel load offset has to be updated in boot_sector as well
	@# emulations supported can be 
	ld -o $@ -Ttext 0x1000 $^

kernel_entry32.o: kernel_entry32.asm
	nasm $< -f elf32 -o $@

kernel_entry64.o: kernel_entry64.asm
	nasm $< -f elf64 -o $@

kernel32.o: kernel.c
	gcc -m32 -ffreestanding -c $< -o $@


kernel64.o: kernel.c
	gcc -m64 -ffreestanding -c $< -o $@

os-image.bin: boot_sector.bin kernel64.bin
	# just concatenate the binaries
	cat $^ > $@

view: os-image.bin
	xxd $< | less

run: os-image.bin
	# simple format was: qemu-system-x86_64 $?
	qemu-system-x86_64 -drive file=$?,format=raw,index=0,media=disk 
    
clean:
	rm -f *.o *.bin



