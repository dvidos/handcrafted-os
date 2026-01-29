
include config.mk
TOP=$(abspath .)
BUILD_DIR=$(TOP)/build
ROOTFS_DIR=$(BUILD_DIR)/rootfs



config.mk:
	./configure.mk

stage1:
	make -C stage1

stage2:
	make -C stage2

kernel:
	make -C kernel

libc:
	make -C libc

apps:
	make -C apps

image/os.img:
	make -C image



clean:
