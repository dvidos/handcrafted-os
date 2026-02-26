#include "../../include/ctypes.h"
#include "../../include/uapi/errors.h"
#include "block_device.h"
#include "../memory/kheap.h"
#include "../klib/string.h"


block_device_t *create_block_device(
    const char *id, 
    const char *name,
    size_t total_blocks,
    size_t block_size,
    struct block_device_ops *ops,
    void *private_data
) {
    block_device_t *d = (block_device_t *)kmalloc(sizeof(block_device_t));
    memset(d, 0, sizeof(block_device_t));

    d->id = strdup(id);
    d->name = strdup(name);
    d->total_blocks = total_blocks;
    d->block_size = block_size;
    d->ops = ops;
    d->priv_data = private_data;
    
    return d;
}

void destroy_block_device(block_device_t *dev) {
    // this is supposed to be called by the child class,
    // it is responsible for freeing private data first
    if (dev) {
        if (dev->id) kfree((void *)dev->id);
        if (dev->name) kfree((void *)dev->name);
        kfree(dev);
    }
}
