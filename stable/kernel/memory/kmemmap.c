#include "kmemmap.h"
#include "../logger/logger.h"

MODULE("KMM", LOG_LEVEL_INFO);


// global
kernel_memory_map_t kmm;


static void kmm_log_info_region(mem_region_t *reg) {
    char flags[64];

    if (reg == NULL) {
        log_info("    From        To                Size    KB  Flags             Usage");
    } else {
        mem_region_describe_flags(reg, flags);

        log_info("    0x%08x  0x%08x  %10lu  %4d  %-16s  %s",
            reg->address,
            reg->address + reg->size - 1,
            reg->size,
            reg->size / 1024,
            flags,
            mem_region_usage_name(reg)
        );
    }
}


void kmm_log_info() {
    log_info("Kernel memory map");

    log_info("- machine maximum memory address 0x%08x.%08x (%u KB, %u MB, %u GB)",
        (uint32_t)(kmm.machine_max_memory_address >> 32),
        (uint32_t)(kmm.machine_max_memory_address & 0xFFFFFFFF),
        (uint32_t)(kmm.machine_max_memory_address / 1024),
        (uint32_t)(kmm.machine_max_memory_address / (1024 * 1024)),
        (uint32_t)(kmm.machine_max_memory_address / (1024 * 1024 * 1024))
    );

    // we decide this arbitrarily (up to 128 MB, where user programs want to load)
    log_info("- reserve area is 0x%x..0x%x (%d..%d KB, %d..%d MB)", 
        kmm.reserved_start, kmm.reserved_end, 
        kmm.reserved_start / KB, kmm.reserved_end / KB, 
        kmm.reserved_start / MB, kmm.reserved_end / MB);

    log_info("- regions");
    kmm_log_info_region(NULL);
    kmm_log_info_region(&kmm.code);
    kmm_log_info_region(&kmm.data);
    kmm_log_info_region(&kmm.bss);
    kmm_log_info_region(&kmm.rodata);
    kmm_log_info_region(&kmm.stack);
    kmm_log_info_region(&kmm.mapping_pages);
    kmm_log_info_region(&kmm.pmm_bitmap);
    kmm_log_info_region(&kmm.heap);
}