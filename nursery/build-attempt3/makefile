#EMU := qemu-system-x86_64
EMU := qemu-system-i386


# run configure.sh to generate the config.mk file
include config.mk

.PHONY: tools stage1 stage2 kernel libc userapps rootfs


default: os.img

config.mk: ./tools/configure.sh
	./tools/configure.sh

config.sh: ./tools/configure.sh
	./tools/configure.sh

config.h: ./tools/configure.sh
	./tools/configure.sh

tools:
	make -C tools

stage1:
	make -C stage1
	mkdir -p build/kernel
	cp stage1/stage1.bin -t build/kernel

stage2:
	make -C stage2
	mkdir -p build/kernel
	cp stage2/stage2.bin -t build/kernel

kernel:
	make -C kernel
	mkdir -p build/kernel
	cp kernel/kernel.bin -t build/kernel

libc:
	make -C libc
	mkdir -p build/rootfs/usr/lib
	cp libc/libc.a -t build/rootfs/usr/lib

userapps: libc
	make -C userapps

rootfs: userapps
	tools/make-root-fs.sh

os.img: stage1 stage2 kernel tools rootfs
	tools/make-os-img.sh

run: run-sata

run-debugger: os.img
	$(EMU) -s -S \
		-drive format=raw,file=os.img,if=none,id=disk0 \
		-device ahci,id=ahci0 \
		-device ide-hd,drive=disk0,bus=ahci0.0 \
		-serial stdio -m 4G

# SATA drive
run-sata: os.img
	$(EMU) \
		-drive format=raw,file=os.img,if=none,id=disk0 \
		-device ahci,id=ahci0 \
		-device ide-hd,drive=disk0,bus=ahci0.0 \
		-serial stdio -m 4G

# ATA / IDE drive
run-ide: os.img  
	$(EMU) -drive format=raw,file=os.img -serial stdio -m 4G


# lots of diagnostics about the CPU
run-with-cpu-diags: os.img
	$(EMU) -drive format=raw,file=os.img -m 4G -monitor stdio -d int,cpu_reset,guest_errors -no-reboot -no-shutdown

clean: config.mk
	make -C tools    clean
	make -C libc     clean
	make -C userapps clean
	make -C stage1   clean
	make -C stage2   clean
	make -C kernel   clean
	rm -rf build/*
	rm -f config.mk config.h config.sh os.img
