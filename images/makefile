
IMAGE_WITH_OWN_BOOT_LOADER = hcos.img
IMAGE_WITH_GRUB = hcos-grub.img

KERNEL_BIN = ../src/kernel/kernel.bin
BOOT_SECTOR_BIN = ../src/boot_sector/boor_sector.bin
KERNEL_LOADER_BIN = ../src/kernel_loader/kernel_loader.bin
LIBC_BIN = ../src/libc/libc.a
LIBC_HEADERS_DIR = ../src/libc/include
USER_BINARIES_DIR = ../user/bin


# make a temp directory and collect everything,
# or, mount the blank image and copy everything there.


all: $(IMAGE_WITH_OWN_BOOT_LOADER) $(IMAGE_WITH_GRUB)


clean:
	@# clear temp directory?

copy_to_usb_stick:
	dd if=$(IMAGE_WITH_GRUB) of=/dev/sda && sync

$(IMAGE_WITH_OWN_BOOT_LOADER): ...


$(IMAGE_WITH_GRUB): ...


install-headers:
	mkdir -p $(SYSROOT_DIR)/usr/include
	cp -R --preserve=timestamps include/. $(SYSROOT_DIR)/usr/include

install-libs: libc.a libk.a
	mkdir -p $(SYSROOT_DIR)/usr/lib
	cp -R --preserve=timestamps libc.a libk.a $(SYSROOT_DIR)/usr/lib

install-user-bins:
	mkdir -p $(SYSROOT_DIR)/bin
	cp -R --preserve=timestamps $(UTILS_BIN_FILES) $(SYSROOT_DIR)/bin

