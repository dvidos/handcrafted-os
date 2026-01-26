#!/bin/sh

rm -f test.img

# create the image
./sfs create --image test.img --size 8M
# Created image 'test.img' with size 8388608 bytes


# make the filesystem, auto block size based on file size
./sfs mkfs --image test.img --start-sector 64 --label TESTFS
# Successfully created SFS filesystem on 'test.img' starting at sector 64 with label 'TESTFS'.


# make a folder to store things in
./sfs mkdir /src --image test.img --start-sector 64
# Successfully created directory '/src' on filesystem at sector 64.


# import some files
./sfs import --image test.img --start-sector 64 ./sfs.c /src/sfs.c
# Successfully imported './sfs.c' (host) to '/src/sfs.c' (SFS) with 30486 bytes.
./sfs import --image test.img --start-sector 64 ./utils.c /src/utils.c
# Successfully imported './utils.c' (host) to '/src/utils.c' (SFS) with 2969 bytes.


# check directory contents
./sfs ls /src --image test.img --start-sector 64
# Contents of '/src' (filesystem at sector 64):
#   .
#   ..
#   sfs.c
#   utils.c


# import a sector, will pad with zeros, or warn if not fitting
./sfs wrsect --image test.img --sector 1 --file makefile
# Warning: Source file 'makefile' (235 bytes) is smaller than target write size (1024 bytes). Padding with zeros.
# Wrote 2 sectors (total 1024 bytes) starting from sector 1 of 'test.img' from 'makefile'.

#read a sector
./sfs rdsect --image test.img --sector 1
# --- Hexdump of 1 sectors starting from 1 ---
# 00000200: 53 52 43 53 20 3d 20 73 66 73 2e 63 20 66 69 6c  |SRCS = sfs.c fil|
# 00000210: 65 5f 73 65 63 74 6f 72 5f 64 65 76 69 63 65 2e  |e_sector_device.|
# 00000220: 63 20 70 61 72 74 69 74 69 6f 6e 5f 73 65 63 74  |c partition_sect|
# 00000230: 6f 72 5f 64 65 76 69 63 65 2e 63 20 75 74 69 6c  |or_device.c util|
# 00000240: 73 2e 63 20 63 6f 6d 6d 61 6e 64 5f 70 61 72 73  |s.c command_pars|
# 00000250: 65 72 2e 63 20 2e 2e 2f 73 72 63 2f 73 69 6d 70  |er.c ../src/simp|
# 00000260: 6c 65 5f 66 69 6c 65 73 79 73 74 65 6d 2e 63 20  |le_filesystem.c |

