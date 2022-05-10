#ifndef _MEMORY_H
#define _MEMORY_H

#include "multiboot.h"


// naive effort, needs to rewrite completely
// void init_memory(unsigned int boot_magic, multiboot_info_t* mbi, uint32_t kernel_start, uint32_t kernel_end);
// void *malloc(size_t size);
// void free(void *address);


// kernel pages minimum functionality
void init_kernel_pages(size_t start, size_t end);
size_t kernel_page_size();
void *allocate_kernel_page();
void free_kernel_page(void *address);



#endif
