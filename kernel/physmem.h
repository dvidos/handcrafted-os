#ifndef _PHYSMEM_H
#define _PHYSMEM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "multiboot.h"


void init_physical_memory_manager(multiboot_info_t *info);
void *allocate_physical_page();
void *allocate_consecutive_physical_pages(size_t size_in_bytes);
void free_physical_page(void *address);
void dump_physical_memory_map();



#endif