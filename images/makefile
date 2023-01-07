
NATIVE_DISK = hcos.img
GRUB_RESCUE_DISK = hcos-rescue.iso

KERNEL_BIN = ../src/kernel/kernel.bin
BOOT_SECTOR_BIN = ../src/boot_sector/boot_sector.bin
BOOT_LOADER_BIN = ../src/boot_loader/boot_loader.bin
USER_BINARIES = $(wildcard ../src/user/bin/*)

LOOP_DEV = /dev/loop100



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

	# create loop device, setup loopback
	[ -b $(LOOP_DEV) ] || sudo mknod $(LOOP_DEV) b 7 100
	sudo losetup -P $(LOOP_DEV) $@

	# generate the primary partition
	# fsisk /dev/loop101
	# o -- create dos partition table
	# n -- create new partition
	# p -- primary (as opposed to e=extended)
	# 1 -- partition number (e.g. 1-4)
	# 2048 -- first sector, 2048 is the min, means 1MB into the disk
	# <enter> -- last sector, last is proposed
	# a   toggle a bootable flag
	# w -- write (but it failed)
	(echo o; \
	 echo n; echo p; echo 1; echo 2048; echo "";  \
	 echo "a";  \
	 echo "p"; echo "w" \
	) | sudo fdisk $(LOOP_DEV) > /dev/null

	sudo fdisk -l $(LOOP_DEV)

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

	@read -p '------ Disk mounted on /mnt. Enter to continue. -------' ENTER
	
	# unmount, detach loop device
	sudo umount $(LOOP_DEV)p1
	sudo losetup -d $(LOOP_DEV)


clean:
	@echo Leaving the images in place



# $(BUILD_DIR)/boot/kernel.bin: $(KERNEL_BIN)
# 	[ -d $(dir $@) ] || mkdir -p $(dir $@)
# 	cp --preserve=timestamps $(KERNEL_BIN) $@

# $(BUILD_DIR)/usr/lib/libc.a: $(LIBC_SRC_BIN)
# 	mkdir -f $(dir $@)
# 	cp --preserve=timestamps $(KERNEL_BIN) $@

# $(LIBC_HEADER_FILES_DEST): $(LIBC_HEADER_FILES_DEST)
# 	mkdir -f $(LIBC_HEADERS_DEST_DIR)
# 	cp ... ?



# copy_to_usb_stick:
# 	dd if=$(IMAGE_GRUB_BOOT) of=/dev/sda && sync

# $(IMAGE_WITH_OWN_BOOT_LOADER):


# $(IMAGE_GRUB_BOOT):


# install-headers:
# 	mkdir -p $(SYSROOT_DIR)/usr/include
# 	cp -R --preserve=timestamps include/. $(SYSROOT_DIR)/usr/include

# install-libs: libc.a libk.a
# 	mkdir -p $(SYSROOT_DIR)/usr/lib
# 	cp -R --preserve=timestamps libc.a libk.a $(SYSROOT_DIR)/usr/lib

# install-user-bins:
# 	mkdir -p $(SYSROOT_DIR)/bin
# 	cp -R --preserve=timestamps $(UTILS_BIN_FILES) $(SYSROOT_DIR)/bin



# we have various targets:
# - libc.lib
# - kernel.bin
# - all the utils to be compiled and copied into the sysroot
# - hdd.img
# 
# then we can either:
# - copy hdd.img into a usb stick
# - run hdd.img on qemu

# echo Preparing disk image...
# rm -rf /mnt/hcos/*
# cp -rv sysroot/* /mnt/hcos
# sync


# to make the images use something like the below
# ---------------------------------------------------
# dd if=/dev/zero of=~/file.img bs=1024k count=10	 (10 MB file)
# losetup /dev/loopUserImg imgs/disk10mb-fat16.img    (loopImg allows all r/w/x)
# losetup --detach /dev/loopUserImg                   (allows simple user again.)
# mount /mnt/userImg                     (user mounts /etc/loop101p1, as defined in /etc/fstab)
# umount /mnt/userImg                    (user unmounts)
# cp sysroot/bin/* /mnt/userImg/bin      (copy compiled user programs)
# 

# after each boot I need to do:
#-----------------------------------------
# sudo mknod /dev/loop100 b 7 100
# sudo chown dimitris:dimitris /dev/loop100
# losetup /dev/loop100 ./imgs/disk10mb-fat16.img 
# mount /mnt/userImg
# cp -rv sysroot/* /mnt/userImg/




# make a temp directory and collect everything,
# or, mount the blank image and copy everything there.
# dependency:
# 1st image file -> common image + boot codes
# 2nd image file -> common image + grubs
