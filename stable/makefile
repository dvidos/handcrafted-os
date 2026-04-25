#EMU := qemu-system-x86_64
EMU := qemu-system-i386


# run configure.sh to generate the config.inc.mk file
include config.inc.mk


.PHONY: tools stage1 stage2 kernel libc userapps rootfs


default: image


config.inc.mk: ./tools/configure.sh
	./tools/configure.sh

config.inc.sh: ./tools/configure.sh
	./tools/configure.sh

config.inc.h: ./tools/configure.sh
	./tools/configure.sh

stage1:
	make -C stage1
	make -C stage1 install

stage2:
	make -C stage2
	make -C stage2 install

kernel:
	make -C kernel
	make -C kernel install
	make -C kernel install_headers

libc:
	make -C libc
	make -C libc install
	make -C libc install_headers

userapps:
	make -C userapps
	make -C userapps install

tools:
	make -C tools

rootfs:
	tools/make-root-fs.sh

image: rootfs stage1 stage2 kernel libc userapps tools
	tools/make-os-img.sh

run: run-sata
#run: run-with-cpu-diags


run-debugger: image
	$(EMU) -s -S \
		-drive format=raw,file=os.img,if=none,id=disk0 \
		-device ahci,id=ahci0 \
		-device ide-hd,drive=disk0,bus=ahci0.0 \
		-serial stdio -m 4G

# SATA drive
run-sata: image
	$(EMU) \
		-drive format=raw,file=os.img,if=none,id=disk0 \
		-device ahci,id=ahci0 \
		-device ide-hd,drive=disk0,bus=ahci0.0 \
		-serial stdio -m 4G

# ATA / IDE drive
run-ide: image  
	$(EMU) -drive format=raw,file=os.img -serial stdio -m 4G


# lots of diagnostics about the CPU
run-with-cpu-diags: image
	$(EMU) -drive format=raw,file=os.img -m 4G -serial stdio -d int,cpu_reset,guest_errors -no-reboot -no-shutdown -D qemu.log

clean: config.inc.mk
	make -C tools    clean
	make -C libc     clean
	make -C userapps clean
	make -C stage1   clean
	make -C stage2   clean
	make -C kernel   clean
	rm -rf ./build/*
	rmdir ./build
	rm -f config.inc.mk config.inc.h config.inc.sh os.img
