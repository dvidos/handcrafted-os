#pragma once
#include <ctypes.h>
#include <uapi/errors.h>
#include "../klib/list.h"


typedef struct block_device block_device_t;

// interface that represents a block (storage) device.
struct block_device {
    const char *id;
    const char *name;
    size_t total_blocks;
    size_t block_size;

    struct block_device_ops *ops;
    void *priv_data;

    list_node_t list_node; // to participate in the devices list
};

struct block_device_ops {
    error_t (*read)(block_device_t *dev, uint64_t lba, uint32_t count, void *buffer);
    error_t (*write)(block_device_t *dev, uint64_t lba, uint32_t count, const void *buffer);
    error_t (*flush)(block_device_t *dev);
    
    void (*destroy)(block_device_t *dev);
};


block_device_t *create_block_device(
    const char *id, 
    const char *name,
    size_t total_blocks,
    size_t block_size,
    struct block_device_ops *ops,
    void *private_data
);
void destroy_block_device(block_device_t *dev);
