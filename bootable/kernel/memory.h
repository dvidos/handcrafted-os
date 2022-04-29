#ifndef _MEMORY_H
#define _MEMORY_H

#include "multiboot.h"


void init_memory(unsigned int boot_magic, multiboot_info_t* mbi, uint32_t kernel_start, uint32_t kernel_end);



#endif
