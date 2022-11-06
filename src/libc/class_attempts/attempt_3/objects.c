#include <ctypes.h>
#include <va_list.h>
#include <stdlib.h>
#include <string.h>
#include "objects.h"


void *new(struct object_info *info, ...) {
    if (info->magic != OBJECT_INFO_MAGIC)
        return NULL;

    void *instance = malloc(info->size);

    // the first member of an instance must be a pointer to an object_info structure
    *(struct object_info **)instance = info;

    // if class supports it, do the special treatment
    if (info->constructor) {
        va_list args;
        va_start(args, info);
        info->constructor(instance, args);
        va_end(args);
    }

    return instance;
}

void delete(void *instance) {
    // the first member of an instance must be a pointer to an object_info structure
    struct object_info *info = *(struct object_info **)instance;
    if (info->magic != OBJECT_INFO_MAGIC)
        return;

    // if class supports it, do the special treatment
    if (info->destructor)
        info->destructor(instance);

    free(instance);
}


void *clone(void *instance) {
    // the first member of an instance must be a pointer to an object_info structure
    struct object_info *info = *(struct object_info **)instance;
    if (info->magic != OBJECT_INFO_MAGIC)
        return NULL;

    // if class supports it, do the special treatment
    if (info->clone)
        return info->clone(instance);

    // otherwise, produce a shallow copy
    void *ptr = malloc(info->size);
    memcpy(ptr, instance, info->size);
    return ptr;
}

bool equals(void *instance_a, void *instance_b) {
    if (instance_a == NULL && instance_b == NULL)
        return true;
    
    if (instance_a == NULL || instance_b == NULL)
        return false; // at least one is not null

    if (instance_a == instance_b)
        return true;
    
    // the first member of an instance must be a pointer to an object_info structure
    struct object_info *info_a = *(struct object_info **)instance_a;
    struct object_info *info_b = *(struct object_info **)instance_b;
    if (info_a->magic != OBJECT_INFO_MAGIC || info_b->magic != OBJECT_INFO_MAGIC)
        return;

    if (info_a != info_b)
        return false;

    // if class supports it, do the special treatment
    if (info_a->equals)
        return info_a->equals(instance_a, instance_b);

    // otherwise, produce a shallow comparison
    return memcmp(instance_a, instance_b, info_a->size) == 0;
}

uint32_t hash(void *instance) {
    // the first member of an instance must be a pointer to an object_info structure
    struct object_info *info = *(struct object_info **)instance;
    if (info->magic != OBJECT_INFO_MAGIC)
        return 0;

    // if class supports it, do the special treatment
    if (info->hash)
        return info->hash(instance);

    // otherwise, produce a shallow copy
    return simple_memory_hash(0, instance, info->size);
}

char *to_string(void *instance) {  // caller to free returned string
    // the first member of an instance must be a pointer to an object_info structure
    struct object_info *info = *(struct object_info **)instance;
    if (info->magic != OBJECT_INFO_MAGIC)
        return NULL;

    // if class supports it, do the special treatment
    if (info->to_string) 
        return info->to_string(instance);

    // otherwise, produce a simple description
    char *name;
    if (info->name) {
        name = malloc(strlen(info->name) + 32);
        sprintfn(name, strlen(info->name) + 32, "%s at 0x%p", info->name, instance);
    } else {
        name = malloc(64);
        sprintfn(name, 64, "Object[0x%p] at 0x%p", info, instance);
    }

    return name;
}

uint32_t simple_memory_hash(uint32_t initial_hash, void *ptr, size_t size) {
    // see http://www.partow.net/programming/hashfunctions/index.html for ideas

    uint32_t hash = initial_hash;
    uint32_t high_nibble;

    while (size-- > 0) {
        hash = (hash << 4) + *(char *)ptr++;
        high_nibble = hash & 0xF0000000L;
        if (high_nibble)
            hash ^= (high_nibble >> 24);

        hash &= ~high_nibble;
    }

    return hash;
}




