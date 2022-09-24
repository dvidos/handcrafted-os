# build process

The objective is to create a .img file that can be used to run the operating system,
either in an emulator, or in a real computer, by burning this image in a usb stick.

## image file contents

The image file contains the following things:

* a file system that is supported by the kernel (e.g. FAT or ext2)
* a boot manager and configuration (Grub for now)
* the kernel binary to load (result of compiling the kernel)
* folders and files to support the operations of the system. these include
  * the kernel binary
  * binary programs (such as `sh` or `vi`)
  * any text files for system configuration (e.g. initrc)
  * any header files, library files or source files for possible development

## how the image file is created

* compile the C library (derive `libc.a`)
* compile the kernel (derive `kernel.bin`)
* compile the user programs (e.g. `sh`)
* mount a disk image file in a loop back device
* sync the mounted folder towards desired structure
* unmount the disk image, it is now ready

## notes

It seems that make's objective it to keep files in sync.
That means that most probably we want to keep the image file 
mounted for the duration of the development, to define that
a file in the mounted image is dependent on a file in the build folders.
Therefore we can have multiple targets in a central makefile?

* make setup - creates and mounts an image
* make [build] - builds and syncs the system, incrementally, towards the image.
  * main development cycle here.
  * running emulator here, with image as hdd.
* make emu - runs emulator
* make teardown - unmounts the image, ready for... publishing?
* make pack - mounts the image, builds, unmounts
* make clean - cleans all projects recursively

## tree structure

* hardcoded-os
  * src - source code
    * libc - makefile and bin folder
    * kernel - makefile and bin folder
    * user - makefile and bin folder
  * sysroot - files to copy to image (e.g. initrc)
    * bin - executable files & utilities
    * boot - kernel to boot and bootloader configuration
    * etc - system configuration
    * usr - include files and libraries
    * var - temp and log files
  * docs - project documentation
  * imgs - image files
