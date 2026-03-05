# makefile to be included in every user project

TARGET = i686-elf
CC = $(TARGET)-gcc
AR = $(TARGET)-ar
LD = $(TARGET)-ld

# find where we are, in order to calculate include & lib paths correctly
CONFIG_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
LIBC_DIR := $(abspath $(CONFIG_DIR)/../libc)
LIBC = $(LIBC_DIR)/libc.a


CFLAGS = \
	-std=gnu99 -ffreestanding -Wall -Wextra -O2 \
	-Wno-unused-parameter \
	-I$(LIBC_DIR)/include

LDFLAGS = -nostdlib -L$(LIBC_DIR) -lc

