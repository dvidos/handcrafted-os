#pragma once
#include <ctypes.h>
#include <uapi/errors.h>
#include "../klib/list.h"


typedef struct char_device char_device_t;

// interface that represents a char device (e.g. pipe, tty, file, serial port, kbd, mouse, etc)
struct char_device {
    const char *id;
    const char *name;

    struct char_device_ops *ops;
    void *priv_data;

    list_node_t list_node; // to participate in the devices list
};

struct char_device_ops {
    ssize_t (*read)(char_device_t *dev, void *buffer, size_t size);
    ssize_t (*write)(char_device_t *dev, const void *buffer, size_t size);
    // ioctl
    
    void (*destroy)(char_device_t *dev);
};

char_device_t *create_char_device(
    const char *id, 
    const char *name,
    struct char_device_ops *ops,
    void *private_data
);
void destroy_char_device(char_device_t *dev);
