#pragma once
// do not edit, run configure.sh to generate
#define VERSION                   "0.1.0"
#define GIT_HASH                  "dd20781-dirty"
#define DATE_BUILT                "2026-02-05 20:24"
#define STAGE1_LOAD_ADDRESS       0x7C00
#define STAGE1_STACK_TOP          0x0000
#define STAGE1_SIZE               512
#define STAGE1_FIRST_SECTOR       0
#define STAGE1_SECTOR_COUNT       1
#define STAGE2_LOAD_ADDRESS       0x0800
#define STAGE2_STACK_TOP          0x7C00
#define STAGE2_SIZE_KB            12
#define STAGE2_FIRST_SECTOR       1
#define STAGE2_SECTOR_COUNT       24
#define KERNEL_LOAD_ADDRESS       0x8000
#define KERNEL_STACK_TOP          0x90000
#define KERNEL_SIZE_KB            128
#define KERNEL_FIRST_SECTOR       25
#define KERNEL_SECTOR_COUNT       256
#define KERNEL_HEAP_ADDRESS       0x100000
#define KERNEL_HEAP_SIZE_KB       4096
#define KERNEL_RAMDISK_ADDRESS    5242880
#define KERNEL_RAMDISK_SIZE_KB    2048
#define SECTOR_SIZE               512
#define DISK_IMAGE_SIZE_MB        8
#define DISK_IMAGE_SIZE_BYTES     8388608
#define DISK_IMAGE_SECTOR_COUNT   16384
#define PARTITION_1_FIRST_SECTOR  281
#define PARTITION_1_SECTOR_COUNT  16103
