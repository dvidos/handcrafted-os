include ../config.inc.mk

STAGE2_OBJS = \
	stage2_asm.o \
	stage2.o




all: stage2.bin

stage2_asm.o: stage2.asm
	nasm $<  -f elf32 -o $@ \
		-DSTAGE2_STACK_TOP=$(STAGE2_STACK_TOP) \
		-DKERNEL_STACK_TOP=$(KERNEL_STACK_TOP) \
		-DKERNEL_LOAD_ADDRESS=$(KERNEL_LOAD_ADDRESS)

stage2.o: stage2.c
	$(CROSS_CC) -m16 -ffreestanding -fno-pie -O2 -c $< -o $@ \
		-DKERNEL_LOAD_ADDRESS=$(KERNEL_LOAD_ADDRESS) \
		-DKERNEL_FIRST_SECTOR=$(KERNEL_FIRST_SECTOR) \
		-DKERNEL_SECTOR_COUNT=$(KERNEL_SECTOR_COUNT)

stage2.elf: $(STAGE2_OBJS)
	$(CROSS_LD) -Ttext=$(STAGE2_LOAD_ADDRESS) -e _stage2_start -o $@ $(STAGE2_OBJS)

stage2.bin: stage2.elf
	objcopy -O binary -S $< $@

clean:
	rm -f stage2.bin stage2.elf *.o
