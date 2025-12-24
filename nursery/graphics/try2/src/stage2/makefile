# caller must define these
STAGE2_LOAD_ADDRESS     ?= 0xBAD
STAGE2_STACK_TOP        ?= 0xBAD
KERNEL_LOAD_ADDRESS     ?= 0xBAD
KERNEL_STACK_TOP        ?= 0xBAD
KERNEL_FIRST_SECTOR_LBA ?= 333
KERNEL_SECTORS          ?= 333


STAGE2_OBJS = \
	stage2_asm.o \
	stage2.o

target: stage2.elf

stage2_asm.o: stage2.asm
	nasm $<  -f elf32 -o $@ \
		-DSTAGE2_STACK_TOP=$(STAGE2_STACK_TOP) -DKERNEL_STACK_TOP=$(KERNEL_STACK_TOP) -DKERNEL_LOAD_ADDRESS=$(KERNEL_LOAD_ADDRESS)


stage2.o: stage2.c
	i686-elf-gcc -m16 -ffreestanding -fno-pie -O2 -c $< -o $@ \
		-DKERNEL_LOAD_ADDRESS=$(KERNEL_LOAD_ADDRESS) -DKERNEL_FIRST_SECTOR_LBA=$(KERNEL_FIRST_SECTOR_LBA) -DKERNEL_SECTORS=$(KERNEL_SECTORS)
		

stage2.elf: $(STAGE2_OBJS)
	i686-elf-ld -Ttext=$(STAGE2_LOAD_ADDRESS) -e _stage2_start -o $@ $(STAGE2_OBJS)


