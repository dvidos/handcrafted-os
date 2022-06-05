#ifndef _PHYSMEM_H
#define _PHYSMEM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <multiboot.h>

int physical_page_size();

void init_physical_memory_manager(multiboot_info_t *info, uint8_t *kernel_start_address, uint8_t *kernel_end_address);

void *allocate_physical_page();
void free_physical_page(void *address);

void *allocate_consecutive_physical_pages(size_t size_in_bytes);
void free_consecutive_physical_pages(void *address, size_t size_in_bytes);

void dump_physical_memory_map_overall();
void dump_physical_memory_map_detail(uint32_t start_address);



#endif