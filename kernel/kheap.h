#ifndef _KHEAP_H
#define _KHEAP_H


// naive effort, needs to rewrite completely
void init_kernel_heap(void *start_address, void *end_address);
char *kalloc(size_t size);
void kfree(char *ptr);
void kernel_heap_dump();



#endif
