#ifndef _OBJECTS_H
#define _OBJECTS_H

#include <va_list.h>


struct object_info {
    int size;
    void (*constructor)(void *instance, va_list args);
    void (*destructor)(void *instance);
};


void *new(struct object_info *info, ...);
void delete(struct object_info *info, void *instance);




#endif