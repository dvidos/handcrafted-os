
NATIVE_DISK = hcos.img
GRUB_RESCUE_DISK = hcos-rescue.iso

KERNEL_BIN = ../src/kernel/kernel.bin
BOOT_SECTOR_BIN = ../src/boot_sector/boot_sector.bin
BOOT_LOADER_BIN = ../src/boot_loader/boot_loader.bin
USER_BINARIES = $(wildcard ../src/user/bin/*)

# loop device to be used for accessing the image
LOOP_DEV_NO = 99
LOOP_DEV = /dev/loop$(LOOP_DEV_NO)



images: $(NATIVE_DISK) $(GRUB_RESCUE_DISK)

print:
	@echo -------- kernel -------------
	@echo $(KERNEL_BIN)
	@echo -------- user binaries names ---------------
	@echo $(USER_BINARIES)
	@echo ------ user binaries at temp folder --------------
	@echo $(USER_BINARIES_AT_DEST)


$(GRUB_RESCUE_DISK): $(KERNEL_BIN)
	mkdir -p mkrescue/boot/grub
	cp $(KERNEL_BIN) mkrescue/boot
	echo menuentry "handcrafted-os" { multiboot /boot/kernel.bin } > mkrescue/boot/grub/grub.cfg
	grub-mkrescue -o $@ mkrescue
	rm -rf mkrescue


# we shall create an image file, with a parimary partition and the boot loaders
# we shall mount it, make a FAT16 file system, and copy the needed files
# then we unmount it and clean up. root permission may be needed.
$(NATIVE_DISK): $(BOOT_SECTOR_BIN) $(BOOT_LOADER_BIN) $(KERNEL_BIN) $(USER_BINARIES)

	# create the file, 10MB
	dd if=/dev/zero of=$@ bs=1024 count=10240

	# copy the first boot_sector and the boot_loader in subsequent sectors
	dd if=$(BOOT_SECTOR_BIN) of=$@ bs=512 count=1  seek=0 conv=notrunc
	dd if=$(BOOT_LOADER_BIN) of=$@ bs=512 count=64 seek=1 conv=notrunc

	# first, detach and unlink, ignoring any error
	sudo losetup -d $(LOOP_DEV) || true
	sudo unlink $(LOOP_DEV)     || true

	# create loop device, setup loopback
	sudo mknod $(LOOP_DEV) b 7 $(LOOP_DEV_NO)
	sudo losetup $(LOOP_DEV) $@

	# print the partition table
	sudo sfdisk -l $(LOOP_DEV)

	# create primary partitopn
	echo "1MiB,,0x7F,*" | sudo sfdisk --no-tell-kernel $(LOOP_DEV)

	# print the partition table
	sudo sfdisk -l $(LOOP_DEV)

	# re-setup loop device to discover the partition
	sudo losetup -d $(LOOP_DEV)
	sudo losetup -P $(LOOP_DEV) $@ 

	# create the filesystem
	sudo mkfs.fat -v -F 16 $(LOOP_DEV)p1

	# mount the filesystem so we can copy files
	sudo mount $(LOOP_DEV)p1 /mnt

	# copy files etc
	sudo mkdir -p /mnt/boot /mnt/bin
	sudo cp --preserve=timestamps $(KERNEL_BIN) /mnt/boot
	sudo cp --preserve=timestamps $(USER_BINARIES) /mnt/bin

	@# possibly pause to verify disk contents
	@#read -p '------ Disk mounted on /mnt. Paused. Enter to unmount and cleanup. -------' ENTER
	
	# unmount, detach loop device
	sudo umount $(LOOP_DEV)p1
	sudo losetup -d $(LOOP_DEV)
	sudo unlink $(LOOP_DEV)


clean:
	@echo Leaving the images in place
	# detach and unlink, ignoring error
	sudo losetup -d $(LOOP_DEV) || true
	sudo unlink $(LOOP_DEV)     || true
