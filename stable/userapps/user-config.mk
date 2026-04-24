# makefile to be included in every user project

TARGET = i686-elf
CC = $(TARGET)-gcc
AR = $(TARGET)-ar
LD = $(TARGET)-ld

# find where we are, in order to calculate include & lib paths correctly
CONFIG_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
ROOTFS_DIR = $(abspath $(CONFIG_DIR)/../build/rootfs)
LIBC_DIR := $(ROOTFS_DIR)/usr/lib
LIBC_INCLUDE_DIR := $(ROOTFS_DIR)/usr/include
LIBC = $(LIBC_DIR)/libc.a



CFLAGS = \
	-std=gnu99 -ffreestanding -Wall -Wextra -O2 \
	-Wno-unused-parameter \
	-I$(LIBC_INCLUDE_DIR)

LDFLAGS = -nostdlib -L$(LIBC_DIR) -lc

