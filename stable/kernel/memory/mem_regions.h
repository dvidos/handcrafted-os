#pragma once
#include "../include/ctypes.h"
#include "../include/uapi/errors.h"
#include "../memory/physical/physmem.h"
#include "../memory/mem_region.h"


/**
 * A collection of regions. 
 * Can be from kernel (identity mapped) or a process (virtual addresses)
 * Would help debugging, detecting owners of memory pointers etc.
 */
#define MEM_MAP_MAX_REGIONS   12

typedef struct mem_regions {
    mem_region_t regions[12];
    int count;
    const char *name; // optional, helps debugging
} mem_regions_t;



void mem_regions_add(mem_regions_t *map, mem_region_t region);
bool mem_regions_contains(mem_regions_t *map, uintptr_t address);
const mem_region_t *mem_regions_find_region(mem_regions_t *map, uintptr_t address);
const mem_region_t *mem_regions_find_by_usage(mem_regions_t *map, region_flags_t usage);
uintptr_t mem_regions_top_address(mem_regions_t *map);
void mem_regions_formatter(log_write_stream_t *stream, va_list args);


