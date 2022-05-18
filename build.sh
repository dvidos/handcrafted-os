#!/bin/sh

# we have various targets:
# - libc.lib
# - kernel.bin
# - all the utils to be compiled and copied into the sysroot
# - hdd.img
# 
# then we can either:
# - copy hdd.img into a usb stick
# - run hdd.img on qemu

# stop on first failure
set -e


cd src/libc && make
cd ../..
cd src/kernel && make
cd ../..
cd src/user && make
cd ../..


