include ../config.inc.mk
ifeq ($(strip $(BUILD_DIR)),)
    $(error BUILD_DIR is empty. Check your config.inc.mk)
endif


all: stage1.bin

stage1.bin: stage1.asm
	nasm $< \
		-DSTAGE2_LOAD_ADDRESS=$(STAGE2_LOAD_ADDRESS) \
		-DSTAGE2_SECTORS=$(STAGE2_SECTOR_COUNT) \
		-f bin \
		-o $@

.PHONY: install
install: stage1.bin
	mkdir -p $(BUILD_DIR)/kernel
	cp stage1.bin -t $(BUILD_DIR)/kernel

clean:
	rm -f stage1.bin
