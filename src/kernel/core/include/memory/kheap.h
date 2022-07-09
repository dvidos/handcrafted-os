#ifndef _KHEAP_H
#define _KHEAP_H

#include <stddef.h>


// naive effort, needs to rewrite completely
void init_kernel_heap(size_t heap_size, void *minimum_address);
void *kmalloc(size_t size);
void kfree(void *ptr);
void kernel_heap_dump();



#endif
