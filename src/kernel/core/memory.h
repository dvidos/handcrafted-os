#ifndef _MEMORY_H
#define _MEMORY_H


// kernel pages minimum functionality
void init_kernel_pages(size_t start, size_t end);
size_t kernel_page_size();
void *allocate_kernel_page();
void free_kernel_page(void *address);



#endif
