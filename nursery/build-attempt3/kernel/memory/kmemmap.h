#pragma once
#include "mem_region.h"


typedef struct kernel_memory_map {
    // this area to be identity mapped, no allocations for user procs
    phys_addr_t reserved_start;
    phys_addr_t reserved_end;
    uint64_t machine_max_memory_address;

    // these regions for debugging etc
    mem_region_t code;
    mem_region_t data;
    mem_region_t rodata;
    mem_region_t bss;
    mem_region_t stack;
    mem_region_t mapping_pages;
    mem_region_t pmm_bitmap;
    mem_region_t heap;

    // here, we could track the extra identity mapped pages
    // such as those used for DMA, MMIO etc.
} kernel_memory_map_t;

extern kernel_memory_map_t kmm;

void kmm_log_info();
