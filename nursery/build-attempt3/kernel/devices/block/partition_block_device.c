#include "partition_block_device.h"
#include "../../include/uapi/errors.h"
#include "../memory/kheap.h"
#include "../klib/string.h"

typedef struct {
    block_device_t *underlying;
    uint64_t first_block;
    size_t num_blocks;
} partition_priv_t;


static error_t partition_read(block_device_t *dev, uint64_t lba, uint32_t count, void *buffer) {
    partition_priv_t *priv = (partition_priv_t *)dev->priv_data;
    if (lba + count > priv->num_blocks) {
        return ERR_INVALID_ARGS;
    }
    return priv->underlying->ops->read(priv->underlying, priv->first_block + lba, count, buffer);
}

static error_t partition_write(block_device_t *dev, uint64_t lba, uint32_t count, const void *buffer) {
    partition_priv_t *priv = (partition_priv_t *)dev->priv_data;
    if (lba + count > priv->num_blocks) {
        return ERR_INVALID_ARGS;
    }
    return priv->underlying->ops->write(priv->underlying, priv->first_block + lba, count, buffer);
}

static error_t partition_flush(block_device_t *dev) {
    partition_priv_t *priv = (partition_priv_t *)dev->priv_data;
    return priv->underlying->ops->flush(priv->underlying);
}

static void partition_destroy(block_device_t *dev) {
    if (dev == NULL) {
        return;
    }
    partition_priv_t *priv = (partition_priv_t *)dev->priv_data;
    kfree(priv);
    destroy_block_device(dev);
}

static struct block_device_ops partition_ops = {
    .read = partition_read,
    .write = partition_write,
    .flush = partition_flush,
    .destroy = partition_destroy,
};

error_t create_partition_block_device(
    const char *id,
    const char *name,
    block_device_t *underlying,
    uint64_t first_block,
    size_t num_blocks,
    block_device_t **result
) {
    if (first_block + num_blocks > underlying->total_blocks)
        return ERR_INVALID_ARGS;

    partition_priv_t *priv = kmalloc(sizeof(partition_priv_t));
    if (priv == NULL)
        return ERR_NO_MEMORY;
    
    priv->underlying = underlying;
    priv->first_block = first_block;
    priv->num_blocks = num_blocks;

    *result = create_block_device(id, name, num_blocks, underlying->block_size, &partition_ops, priv);
    return OK;
}

