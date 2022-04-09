# Common macros
#   $@ is the target for multiple targets (separated by space)
#   $? are the dependencies newer than the target
#   $^ are all the dependences
#
# For implicit rules (e.g. ".c.o:")
#   $< is the target that caused the action
#   $* is the plain filename, common to both extensions


# to see binary files, use xxd tool



# define phony targets:
.PHONY: 

boot_sector: boot_sector.bin



run: boot_sector.bin
	qemu-system-x86_64 $?


view: boot_sector.bin
	xxd $?



boot_sector.bin: boot_sector.asm
	nasm -f bin boot_sector.asm -o boot_sector.bin


