
# get the directory of THIS file (user-config.mk), regardless of where the CWD is.
THIS_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

# include makefile from parent dir of this makefile
include $(THIS_DIR)/../config.inc.mk

# verify everything is loaded
ifeq ($(strip $(BUILD_DIR)),)
    $(error BUILD_DIR is empty. Check your config.inc.mk)
endif

# what to compile / link against
LIBC_LIB_DIR     := $(BUILD_DIR)/rootfs/usr/lib
LIBC_INCLUDE_DIR := $(BUILD_DIR)/rootfs/usr/include
LIBC             := $(LIBC_LIB_DIR)/libc.a

# $(warning Using $(LIBC_INCLUDE_DIR) for includes)
# $(warning Using $(LIBC_LIB_DIR) for linking)


CFLAGS = \
	-std=gnu99 -ffreestanding -Wall -Wextra -Wno-unused-parameter -O2 \
	-I$(LIBC_INCLUDE_DIR)

LDFLAGS = -nostdlib -L$(LIBC_LIB_DIR) -lc

