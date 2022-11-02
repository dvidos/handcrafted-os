#include <va_list.h>
#include <stdlib.h>
#include "objects.h"


void *new(struct object_info *info, ...) {
    void *instance = malloc(info->size);

    if (info->constructor) {
        va_list args;
        va_start(args, info);
        info->constructor(instance, args);
        va_end(args);
    }

    return instance;
}

void delete(struct object_info *info, void *instance) {
    if (info->destructor) {
        info->destructor(instance);
    }

    free(info);
}
