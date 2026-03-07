#pragma once
#include "../include/ctypes.h"
#include "../include/uapi/errors.h"
#include "../memory/virtmem.h"
#include "../memory/physmem.h"


void mem_region_set_util_page_address(phys_addr_t addr);

/**
 * Flags and usage description of a memory region.
 */
typedef enum region_flags {
    REGION_WRITE_ENABLE = 0x01,    // e.g. cannot write
    REGION_USER_ACCESSIBLE = 0x02, // e.g. user procs have access or only kernel
    REGION_GLOBAL_MAP = 0x04,      // e.g. keep cache across different page directories

    REGION_USAGE_CODE  = 0x0101,
    REGION_USAGE_DATA  = 0x0102,
    REGION_USAGE_STACK = 0x0103,
    REGION_USAGE_HEAP  = 0x0104,
    REGION_USAGE_MMIO  = 0x0105, // memory bound io or hardware (e.g. framebuffer)
    REGION_USAGE_SHMEM = 0x0106, // shared memory
    REGION_USAGE_FILE  = 0x0107, // memory-mapped files
    REGION_USAGE_GUARD = 0x0108, // for heap or stack overflows
} region_flags_t;


/**
 * This structure to describe one single-purposed region in memory
 * An array of them define a memory map
 */
typedef struct memory_region {
    uintptr_t address;     // virtual or physical, depends on context
    size_t size;           // size in bytes
    region_flags_t flags;  // purpose and permissions
    const char *name;      // optional, but helpful, pointer not owned.
} memory_region_t;

memory_region_t mem_region_empty();
bool mem_region_is_empty(memory_region_t *reg);


/**
 * Ability to fill a page by clearing, copying, or reading a file.
 */
typedef struct page_filler {
    error_t (*fill_page)(size_t page_num, uintptr_t dest_addr, void *context);
    void *context;
} page_fill_t;


error_t mem_region_allocate_clear_and_map(memory_region_t *reg, page_dir_t target_page_dir);
error_t mem_region_allocate_copy_and_map(memory_region_t *reg, page_dir_t target_page_dir, uintptr_t source_address);
error_t mem_region_allocate_fill_and_map(memory_region_t *reg, page_dir_t target_page_dir, page_fill_t *filler);
error_t mem_region_unmap_and_release(memory_region_t *reg, page_dir_t page_dir);

