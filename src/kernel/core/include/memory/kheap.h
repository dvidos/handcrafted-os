#ifndef _KHEAP_H
#define _KHEAP_H

#include <ctypes.h>


// naive effort, needs to rewrite completely
void init_kernel_heap(size_t heap_size, void *minimum_address);

// define DEBUG_HEAP_OPS to help debugging overflows
void *__kmalloc(size_t size, char *file, uint16_t line);

// checks magic numbers and logs possible overflow/underflow
void __kcheck(void *ptr, char *name, char *file, int line);

void kfree(void *ptr);

void kernel_heap_dump();


#define DEBUG_HEAP_OPS  1
#ifdef DEBUG_HEAP_OPS
    #define kmalloc(size)      __kmalloc(size, __FILE__, __LINE__)
    #define kcheck(ptr, name)  __kcheck(ptr, name, __FILE__, __LINE__)
#else
    #define kmalloc(size)      __kmalloc(size, NULL, 0)
    #define kcheck(ptr, name)  ((void)0)
#endif



#endif
