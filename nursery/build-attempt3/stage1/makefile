include ../config.mk

all: stage1.bin

stage1.bin: stage1.asm
	nasm $< \
		-DSTAGE2_LOAD_ADDRESS=$(STAGE2_LOAD_ADDRESS) \
		-DSTAGE2_SECTORS=$(STAGE2_SECTOR_COUNT) \
		-f bin \
		-o $@

clean:
	rm -f stage1.bin
