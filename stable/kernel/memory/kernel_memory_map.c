#include "kernel_memory_map.h"
#include "../logger/logger.h"

MODULE("KMM", LOG_LEVEL_INFO);



void kmm_log_info(kernel_memory_map_t *kmm) {
    log_info("Kernel memory map");

    log_info("- machine maximum memory address 0x%08x.%08x (%u KB, %u MB, %u GB)",
        (uint32_t)(kmm->machine_max_memory_address >> 32),
        (uint32_t)(kmm->machine_max_memory_address & 0xFFFFFFFF),
        (uint32_t)(kmm->machine_max_memory_address / 1024),
        (uint32_t)(kmm->machine_max_memory_address / (1024 * 1024)),
        (uint32_t)(kmm->machine_max_memory_address / (1024 * 1024 * 1024))
    );

    // we decide this arbitrarily (up to 128 MB, where user programs want to load)
    log_info("- reserve area is 0x%x..0x%x (%d..%d KB, %d..%d MB)", 
        kmm->reserved_start, kmm->reserved_end, 
        kmm->reserved_start / KB, kmm->reserved_end / KB, 
        kmm->reserved_start / MB, kmm->reserved_end / MB);

    log_info("- regions");
    for (int i = 0; i < kmm->regions.count; i++) {
        log_info_fmt(mem_region_formatter, "  - ", &kmm->regions.regions[i]);
    }
}
