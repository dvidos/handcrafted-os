#pragma once
#include "mem_region.h"
#include "mem_regions.h"


typedef struct kernel_memory_map {
    // this area to be identity mapped, no allocations for user procs
    phys_addr_t reserved_start;
    phys_addr_t reserved_end;
    uint64_t machine_max_memory_address;

    // regions on the map
    mem_regions_t regions;

} kernel_memory_map_t;


void kernel_memory_map_formatter(log_write_stream_t *stream, va_list args);




