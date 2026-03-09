#pragma once
#include "../include/ctypes.h"
#include "../include/uapi/errors.h"
#include "../memory/physmem.h"
#include "../memory/mem_region.h"


/**
 * A collection of regions. 
 * Can be from kernel (identity mapped) or a process (virtual addresses)
 * Would help debugging, detecting owners of memory pointers etc.
 */
#define MEM_MAP_MAX_REGIONS   12

typedef struct mem_map {
    mem_region_t regions[12];
    int count;
    const char *name; // optional, helps debugging
} mem_map_t;



void mem_map_add_region(mem_map_t *map, mem_region_t region);
bool mem_map_contains_address(mem_map_t *map, uintptr_t address);
const mem_region_t *mem_map_get_containing_region(mem_map_t *map, uintptr_t address);
uintptr_t mem_map_get_top_address(mem_map_t *map);
void mem_map_formatter(log_write_stream_t *stream, va_list args);


