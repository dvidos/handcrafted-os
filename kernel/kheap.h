#ifndef _KHEAP_H
#define _KHEAP_H


// naive effort, needs to rewrite completely
void init_kernel_heap();
void *kalloc(size_t size);
void kfree(void *ptr);
void kernel_heap_dump();



#endif
