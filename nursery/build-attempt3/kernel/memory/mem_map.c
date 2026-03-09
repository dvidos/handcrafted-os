#include "mem_region.h"
#include "../klib/string.h"
#include "../memory/virtmem.h"
#include "../memory/physmem.h"
#include "../logger/logger.h"
#include "../include/va_list.h"

MODULE("MEM_MAP", LOG_LEVEL_WARN);


void mem_map_formatter(log_write_stream_t *stream, va_list args) {
    mem_map_t *map = va_arg(args, mem_map_t *);
    char flags[64];
    
    if (map->name)
        stream->printf(stream->context, "%s", map->name);

    stream->printf(stream->context, "    No        From          To        Size    KB  Flags             Usage");
    // |  No     Address          To        Size    KB  Flags             Usage
    // |  nn  0x12345678  0x12345678  1234567890  1234  1234567890123456  code
    // |  nn  0x12345678  0x12345678  1234567890  1234  1234567890123456  code
    // |  nn  0x12345678  0x12345678  1234567890  1234  1234567890123456  code

    for (int i = 0; i < map->count; i++) {
        mem_region_t *reg = &map->regions[i];
        mem_region_describe_flags(reg, flags);

        stream->printf(stream->context, "    %2d  0x%08x  0x%08x  %10lu  %4d  %-16s  %s",
            i,
            reg->address,
            reg->address + reg->size - 1,
            reg->size,
            reg->size / 1024,
            flags,
            mem_region_usage_name(reg)
        );
    }
}

void mem_map_add_region(mem_map_t *map, mem_region_t reg) {
    if (map->count >= MEM_MAP_MAX_REGIONS) {
        log_error("Memory map full, cannot add region");
        return;
    }

    map->regions[map->count] = reg;
    map->count += 1;
}

uintptr_t mem_map_get_top_address(mem_map_t *map) {
    uintptr_t last = 0;

    for (int i = 0; i < map->count; i++) {
        uintptr_t region_last = map->regions[i].address + map->regions[i].size - 1;
        if (region_last > last)
            last = region_last;
    }

    return last;
}

bool mem_map_contains_address(mem_map_t *map, uintptr_t address) {
    for (int i = 0; i < map->count; i++) {
        if (mem_region_contains_address(&map->regions[i], address))
            return true;
    }
    return false;
}

const mem_region_t *mem_map_get_containing_region(mem_map_t *map, uintptr_t address) {
    for (int i = 0; i < map->count; i++) {
        if (mem_region_contains_address(&map->regions[i], address))
            return &map->regions[i];
    }
    return NULL;
}
