# run configure.sh to generate the config.mk file
include config.mk

.PHONY: tools stage1 stage2 kernel libc apps rootfs


default: os.img

config.mk:
	./configure.sh

tools:
	make -C tools

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

rootfs: libc apps
	tools/make-root-fs.sh

os.img: tools stage1 stage2 kernel rootfs
	tools/make-os-img.sh

run: os.img
	@#qemu-system-i386 -drive format=raw,file=build/os.img -monitor stdio -d int,cpu_reset,guest_errors -no-reboot -no-shutdown
	qemu-system-i386 -drive format=raw,file=os.img -serial stdio

clean: config.mk
	make -C tools clean
	make -C libc  clean
	make -C apps  clean
	make -C stage1 clean
	make -C stage2 clean
	make -C kernel clean
	rm -rf build/*
	rm -f config.mk
	rm -f os.img
