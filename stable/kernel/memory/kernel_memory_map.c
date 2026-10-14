#include "kernel_memory_map.h"
#include "../logger/logger.h"

MODULE("KMM", LOG_LEVEL_INFO);


void kernel_memory_map_formatter(log_write_stream_t *stream, va_list args) {
    kernel_memory_map_t *map = va_arg(args, kernel_memory_map_t *);

    stream->printf(stream, "Kernel memory map");

    stream->printf(stream, "- machine maximum memory address 0x%08x.%08x (%u KB, %u MB, %u GB)",
        (uint32_t)(map->machine_max_memory_address >> 32),
        (uint32_t)(map->machine_max_memory_address & 0xFFFFFFFF),
        (uint32_t)(map->machine_max_memory_address / 1024),
        (uint32_t)(map->machine_max_memory_address / (1024 * 1024)),
        (uint32_t)(map->machine_max_memory_address / (1024 * 1024 * 1024))
    );

    // we decide this arbitrarily (up to 128 MB, where user programs want to load)
    stream->printf(stream, "- reserve area is 0x%x..0x%x (%d..%d KB, %d..%d MB)", 
        map->reserved_start, map->reserved_end, 
        map->reserved_start / KB, map->reserved_end / KB, 
        map->reserved_start / MB, map->reserved_end / MB);

    stream->printf(stream, "- regions");
    stream->print_fmt(stream, "", mem_regions_formatter, &map->regions);
}
