#include "ram_block_device.h"
#include <memory/kheap.h>
#include <klib/string.h>
#include <uapi/errors.h>

typedef struct {
    size_t block_size;
    size_t block_count;
    char *buffer;
} ram_disk_priv_t;

static void ram_disk_destroy(block_device_t *dev);

static size_t ram_disk_total_blocks(block_device_t *dev) {
    ram_disk_priv_t *priv = (ram_disk_priv_t *)dev->priv_data;
    return priv->block_count;
}

static size_t ram_disk_block_size(block_device_t *dev) {
    ram_disk_priv_t *priv = (ram_disk_priv_t *)dev->priv_data;
    return priv->block_size;
}

static error_t ram_disk_read(block_device_t *dev, uint64_t lba, uint32_t count, void *buffer) {
    ram_disk_priv_t *priv = (ram_disk_priv_t *)dev->priv_data;
    if (lba + count > priv->block_count) {
        return ERR_INVALID_ARGS;
    }
    memcpy(buffer, priv->buffer + lba * priv->block_size, count * priv->block_size);
    return OK;
}

static error_t ram_disk_write(block_device_t *dev, uint64_t lba, uint32_t count, const void *buffer) {
    ram_disk_priv_t *priv = (ram_disk_priv_t *)dev->priv_data;
    if (lba + count > priv->block_count) {
        return ERR_INVALID_ARGS;
    }
    memcpy(priv->buffer + lba * priv->block_size, buffer, count * priv->block_size);
    return OK;
}

static error_t ram_disk_flush(block_device_t *dev) {
    (void)dev;
    return OK;
}

static struct block_device_ops ram_disk_ops = {
    .total_blocks = ram_disk_total_blocks,
    .block_size = ram_disk_block_size,
    .read = ram_disk_read,
    .write = ram_disk_write,
    .flush = ram_disk_flush,
    .destroy = ram_disk_destroy,
};

error_t create_ram_block_device(size_t block_size, size_t block_count, block_device_t **result) {
    ram_disk_priv_t *priv = kmalloc(sizeof(ram_disk_priv_t));
    if (priv == NULL) {
        return ERR_NO_MEMORY;
    }
    priv->block_size = block_size;
    priv->block_count = block_count;
    priv->buffer = kmalloc(block_size * block_count);
    if (priv->buffer == NULL) {
        kfree(priv);
        return ERR_NO_MEMORY;
    }

    block_device_t *dev = kmalloc(sizeof(block_device_t));
    if (dev == NULL) {
        kfree(priv->buffer);
        kfree(priv);
        return ERR_NO_MEMORY;
    }
    dev->ops = &ram_disk_ops;
    dev->priv_data = priv;

    *result = dev;
    return OK;
}

static void ram_disk_destroy(block_device_t *dev) {
    if (dev == NULL) {
        return;
    }
    ram_disk_priv_t *priv = (ram_disk_priv_t *)dev->priv_data;
    kfree(priv->buffer);
    kfree(priv);
    kfree(dev);
}
