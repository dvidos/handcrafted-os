#ifndef _PHYSMEM_H
#define _PHYSMEM_H

#include <ctypes.h>
#include <multiboot.h>

int physical_page_size();

void init_physical_memory_manager(multiboot_info_t *info, void *kernel_start_address, void *kernel_end_address);

void *allocate_physical_page(void *minimum_address);
void free_physical_page(void *address);

void *allocate_consecutive_physical_pages(size_t size_in_bytes, void *minimum_address);
void free_consecutive_physical_pages(void *address, size_t size_in_bytes);

typedef struct phys_mem_info {
    int kb_total;
    int kb_used;
    int kb_free;
    int pages_total;
    int pages_used;
    int pages_free;
} phys_mem_info_t;

void get_physical_memory_info(phys_mem_info_t *info);

void dump_physical_memory_map_overall();
void dump_physical_memory_map_detail(uint32_t start_address);



#endif