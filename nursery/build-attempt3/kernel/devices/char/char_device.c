#include "../../include/ctypes.h"
#include "../../include/uapi/errors.h"
#include "char_device.h"
#include "../memory/kheap.h"
#include "../klib/string.h"


char_device_t *create_char_device(
    const char *id, 
    const char *name,
    struct char_device_ops *ops,
    void *private_data
) {
    char_device_t *d = (char_device_t *)kmalloc(sizeof(char_device_t));
    memset(d, 0, sizeof(char_device_t));

    d->id = strdup(id);
    d->name = strdup(name);
    d->ops = ops;
    d->priv_data = private_data;
    
    return d;
}

void destroy_char_device(char_device_t *dev) {
    // this is supposed to be called by the child class,
    // it is responsible for freeing private data first
    if (dev) {
        if (dev->id) kfree((void *)dev->id);
        if (dev->name) kfree((void *)dev->name);
        kfree(dev);
    }
}
