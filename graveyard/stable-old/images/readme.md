
# images

This folder to contain the produced images, as the output of the whole project.

Those images should be able to be booted, when copied to a USB stick or something.
Also, we should be able to run them on qemu or other emulators.

## two images

Currently I have two images, one with GRUB, one with my own boot loader (which is incomplete)

## custom image structure

My first image contains the following:

* The file is 10MB long.
* First sector (512 bytes) a primitive boot sector and a legacy partition table.
* Second sector and onwards, for 64 sectors, it's the second stage boot loader (or kernel loader)
* The first primary partition starts on sector 1024, 1MB into the image
* This partition has a FAT16 filesystem, with the following contents:
  * bin - contains all the utilties and demos we compiled in the user folder
  * boot - contains the kernel to load (kernel.bin ?)
  * etc - contains the initialization script, initrc and maybe other files
  * usr - contains the C library (libc.a) and C headers
  * var - contains runtime files, logs, etc

## image with GRUB bootloader

Is the same as the first, with one primary partition and the FAT16 filesystem,
but has a completely GRUB installed boot loading system.

