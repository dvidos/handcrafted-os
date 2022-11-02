#include <va_list.h>
#include <stdlib.h>
#include "objects.h"


void *new(struct object_info *info, ...) {
    void *instance = malloc(info->size);

    // the idea is that the first memober of an object 
    // manipulated using this family of methods, 
    // shall be the pointer to object_info.
    *(struct object_info **)instance = info;

    if (info->constructor) {
        va_list args;
        va_start(args, info);
        info->constructor(instance, args);
        va_end(args);
    }

    return instance;
}

void delete(void *instance) {

    // the idea is that the first memober of an object 
    // manipulated using this family of methods, 
    // shall be the pointer to object_info.
    struct object_info *info = *(struct object_info **)instance;

    if (info->destructor) {
        info->destructor(instance);
    }

    free(instance);
}



