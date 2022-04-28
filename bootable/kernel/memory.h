#ifndef _MEMORY_H
#define _MEMORY_H

#include "multiboot.h"

void init_memory(unsigned int boot_magic, multiboot_info_t* mbi, void *kernel_start, void *kernel_end);





#endif
