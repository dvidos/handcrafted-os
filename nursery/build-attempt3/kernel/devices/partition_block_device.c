#include "partition_block_device.h"
#include <uapi/errors.h>
#include "../memory/kheap.h"
#include "../klib/string.h"

typedef struct {
    char *name;
    block_device_t *underlying;
    uint64_t first_block;
    size_t num_blocks;
} partition_priv_t;

static void partition_destroy(block_device_t *dev);
static const char *partition_name(block_device_t *dev);

static size_t partition_total_blocks(block_device_t *dev) {
    partition_priv_t *priv = (partition_priv_t *)dev->priv_data;
    return priv->num_blocks;
}

static size_t partition_block_size(block_device_t *dev) {
    partition_priv_t *priv = (partition_priv_t *)dev->priv_data;
    return priv->underlying->ops->block_size(priv->underlying);
}

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

static struct block_device_ops partition_ops = {
    .name = partition_name,
    .total_blocks = partition_total_blocks,
    .block_size = partition_block_size,
    .read = partition_read,
    .write = partition_write,
    .flush = partition_flush,
    .destroy = partition_destroy,
};

error_t create_partition_block_device(
    const char *name,
    block_device_t *underlying,
    uint64_t first_block,
    size_t num_blocks,
    block_device_t **result
) {
    partition_priv_t *priv = kmalloc(sizeof(partition_priv_t));
    if (priv == NULL) {
        return ERR_NO_MEMORY;
    }
    priv->name = strdup(name);
    if (priv->name == NULL) {
        kfree(priv);
        return ERR_NO_MEMORY;
    }
    priv->underlying = underlying;
    priv->first_block = first_block;
    priv->num_blocks = num_blocks;

    if (first_block + num_blocks > underlying->ops->total_blocks(underlying)) {
        kfree(priv->name);
        kfree(priv);
        return ERR_INVALID_ARGS;
    }

    block_device_t *dev = kmalloc(sizeof(block_device_t));
    if (dev == NULL) {
        kfree(priv->name);
        kfree(priv);
        return ERR_NO_MEMORY;
    }
    dev->ops = &partition_ops;
    dev->priv_data = priv;

    *result = dev;
    return OK;
}

static void partition_destroy(block_device_t *dev) {
    if (dev == NULL) {
        return;
    }
    partition_priv_t *priv = (partition_priv_t *)dev->priv_data;
    kfree(priv->name);
    kfree(priv);
    kfree(dev);
}



static const char *partition_name(block_device_t *dev) {
    partition_priv_t *priv = (partition_priv_t *)dev->priv_data;
    return priv->name;
}
