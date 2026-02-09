#pragma once
#include <ctypes.h>
#include <uapi/errors.h>


typedef struct block_device block_device_t;

struct block_device {
    struct block_device_ops *ops;
    void *priv_data;
};

struct block_device_ops {
    size_t (*total_blocks)(block_device_t *dev);
    size_t (*block_size)(block_device_t *dev);
    error_t (*read)(block_device_t *dev, uint64_t lba, uint32_t count, void *buffer);
    error_t (*write)(block_device_t *dev, uint64_t lba, uint32_t count, const void *buffer);
    error_t (*flush)(block_device_t *dev);
    void (*destroy)(block_device_t *dev);
};
