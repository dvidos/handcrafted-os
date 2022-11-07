#ifndef _OBJECTS_H
#define _OBJECTS_H

#include <ctypes.h>
#include <va_list.h>


#define OBJECT_INFO_MAGIC   0x7BD71F17

// must be a pointer to an class_info structure 
struct class_info {
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

typedef void *(memory_allocator)(size_t size);
typedef void (memory_deallocator)(void *ptr);

void init_objects_runtime(memory_allocator *allocator, memory_deallocator *deallocator);


// the caller must cast the returned value to the proper struct pointer...
void *new(struct class_info *info, ...);
void delete(void *instance);
void *clone(void *instance);
bool equals(void *instance_a, void *instance_b);
uint32_t hash(void *instance);
char *to_string(void *instance);  // caller to free returned string
size_t size_of(void *instance);
bool instance_of(void *instance, struct class_info *info);
struct class_info *class_of(void *instance); // returned value not to be freed


uint32_t simple_memory_hash(uint32_t initial_hash, void *ptr, size_t size);


#endif
