#ifndef _OBJECTS_H
#define _OBJECTS_H

#include <ctypes.h>
#include <va_list.h>


#define OBJECT_INFO_MAGIC   0x7BD71F17

// must be a pointer to an object_info structure 
struct object_info {
    int size;
    char *name;
    int magic; // should be OBJECT_INFO_MAGIC
    void (*constructor)(void *instance, va_list args);
    void (*destructor)(void *instance);
    void *(*clone)(void *instance);
    bool (*equals)(void *instance_a, void *instance_b);
    uint32_t (*hash)(void *instance);
    char *(*to_string)(void *instance); // caller should free memory
};


// the caller must cast the returned value to the proper struct pointer...
void *new(struct object_info *info, ...);
void delete(void *instance);
void *clone(void *instance);
bool equals(void *instance_a, void *instance_b);
uint32_t hash(void *instance);
char *to_string(void *instance);  // caller to free returned string


uint32_t simple_memory_hash(uint32_t initial_hash, void *ptr, size_t size);


#endif
