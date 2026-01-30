include ../config.mk

OUTDIR=../build/kernel
OUT=$(OUTDIR)/stage1.bin

all: $(OUT)

$(OUT): stage1.asm
	mkdir -p $(OUTDIR)
	nasm stage1.asm \
		-DSTAGE2_LOAD_ADDRESS=$(STAGE2_LOAD_ADDRESS) \
		-DSTAGE2_SECTORS=$(STAGE2_SECTOR_COUNT) \
		-f bin \
		-o $(OUT)


clean:
	rm -f stage1.bin
