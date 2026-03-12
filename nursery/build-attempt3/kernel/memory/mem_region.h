#pragma once
#include "../include/ctypes.h"
#include "../include/uapi/errors.h"
#include "../memory/physmem.h"
#include "../logger/logger.h"


/**
 * This is a virt address to be used for copying pages, one page at a time
 */
void mem_region_set_mappable_page_address(phys_addr_t addr);
phys_addr_t mem_region_get_mappable_page_address();

/**
 * Flags and usage description of a memory region.
 */
typedef enum region_flags {
    REGION_READ_ONLY       = 0x00,
    REGION_WRITE_ENABLE    = 0x01,

    REGION_SUPERVISOR_ONLY = 0x00,
    REGION_USER_ACCESSIBLE = 0x02,   // e.g. user procs have access or only kernel

    REGION_LOCAL_MAP       = 0x00,
    REGION_GLOBAL_MAP      = 0x04,   // e.g. keep cache across different page directories

    REGION_USAGE_MASK      = 0x00F0,
    REGION_USAGE_CODE      = 0x0010,
    REGION_USAGE_DATA      = 0x0020,
    REGION_USAGE_STACK     = 0x0030,
    REGION_USAGE_HEAP      = 0x0040,
    REGION_USAGE_MMIO      = 0x0050, // memory bound io or hardware (e.g. framebuffer)
    REGION_USAGE_SHMEM     = 0x0060, // shared memory
    REGION_USAGE_FILE      = 0x0070, // memory-mapped files
    REGION_USAGE_GUARD     = 0x0080, // for heap or stack overflows
} region_flags_t;


/**
 * This structure to describe one single-purposed region in memory
 * An array of them define a memory map
 */
typedef struct mem_region {
    uintptr_t address;     // virtual or physical, depends on context
    size_t size;           // size in bytes
    region_flags_t flags;  // purpose and permissions
    const char *name;      // optional, but helpful, pointer not owned.
} mem_region_t;

static inline mem_region_t mem_region_empty() { return (mem_region_t){ .address = 0, .size = 0, .flags = 0, .name = 0 }; }
static inline mem_region_t mem_region_of(uintptr_t address, size_t size, region_flags_t flags, const char *name) { return (mem_region_t){ .address = address, .size = size, .flags = flags, .name = name }; }

static inline mem_region_t mem_region_kernel_code(uintptr_t address, size_t size)   { return (mem_region_t){ .address = address, .size = size, .flags = REGION_SUPERVISOR_ONLY | REGION_READ_ONLY | REGION_USAGE_CODE }; }
static inline mem_region_t mem_region_kernel_data(uintptr_t address, size_t size)   { return (mem_region_t){ .address = address, .size = size, .flags = REGION_SUPERVISOR_ONLY | REGION_WRITE_ENABLE | REGION_USAGE_DATA }; }
static inline mem_region_t mem_region_kernel_rodata(uintptr_t address, size_t size) { return (mem_region_t){ .address = address, .size = size, .flags = REGION_SUPERVISOR_ONLY | REGION_READ_ONLY | REGION_USAGE_DATA, .name = "rodata" }; }
static inline mem_region_t mem_region_kernel_bss(uintptr_t address, size_t size)    { return (mem_region_t){ .address = address, .size = size, .flags = REGION_SUPERVISOR_ONLY | REGION_WRITE_ENABLE | REGION_USAGE_DATA, .name = "bss" }; }
static inline mem_region_t mem_region_kernel_stack(uintptr_t address, size_t size)  { return (mem_region_t){ .address = address, .size = size, .flags = REGION_SUPERVISOR_ONLY | REGION_WRITE_ENABLE | REGION_USAGE_STACK }; }
static inline mem_region_t mem_region_kernel_heap(uintptr_t address, size_t size)   { return (mem_region_t){ .address = address, .size = size, .flags = REGION_SUPERVISOR_ONLY | REGION_WRITE_ENABLE | REGION_USAGE_HEAP }; }
static inline mem_region_t mem_region_kernel_other(uintptr_t address, size_t size, const char *name) { return (mem_region_t){ .address = address, .size = size, .flags = REGION_SUPERVISOR_ONLY | REGION_WRITE_ENABLE, .name = name }; }

static inline mem_region_t mem_region_user_code(uintptr_t address, size_t size)     { return (mem_region_t){ .address = address, .size = size, .flags = REGION_USER_ACCESSIBLE | REGION_READ_ONLY | REGION_USAGE_CODE }; }
static inline mem_region_t mem_region_user_data(uintptr_t address, size_t size)     { return (mem_region_t){ .address = address, .size = size, .flags = REGION_USER_ACCESSIBLE | REGION_WRITE_ENABLE | REGION_USAGE_DATA }; }
static inline mem_region_t mem_region_user_rodata(uintptr_t address, size_t size)   { return (mem_region_t){ .address = address, .size = size, .flags = REGION_USER_ACCESSIBLE | REGION_READ_ONLY | REGION_USAGE_DATA }; }
static inline mem_region_t mem_region_user_bss(uintptr_t address, size_t size)      { return (mem_region_t){ .address = address, .size = size, .flags = REGION_USER_ACCESSIBLE | REGION_WRITE_ENABLE | REGION_USAGE_DATA, .name = "bss" }; }
static inline mem_region_t mem_region_user_stack(uintptr_t address, size_t size)    { return (mem_region_t){ .address = address, .size = size, .flags = REGION_USER_ACCESSIBLE | REGION_WRITE_ENABLE | REGION_USAGE_STACK }; }
static inline mem_region_t mem_region_user_heap(uintptr_t address, size_t size)     { return (mem_region_t){ .address = address, .size = size, .flags = REGION_USER_ACCESSIBLE | REGION_WRITE_ENABLE | REGION_USAGE_HEAP }; }
static inline mem_region_t mem_region_user_other(uintptr_t address, size_t size, const char *name) { return (mem_region_t){ .address = address, .size = size, .flags = REGION_USER_ACCESSIBLE | REGION_WRITE_ENABLE, .name = name }; }

static inline bool mem_region_is_empty(mem_region_t *reg) { if (reg == NULL) return true; return (reg->address == 0 && reg->size == 0 && reg->flags == 0 && reg->name == 0); }
const char *mem_region_usage_name(mem_region_t *reg);
void mem_region_describe_flags(mem_region_t *reg, char *buffer);
void mem_region_formatter(log_write_stream_t *stream, va_list args);
bool mem_region_contains_address(mem_region_t *reg, uintptr_t address);


/**
 * Ability to fill a page by clearing, copying, or reading a file.
 */
typedef struct page_filler {
    error_t (*fill_page)(size_t page_num, uintptr_t dest_addr, void *context);
    void *context;
} page_filler_t;


error_t mem_region_allocate_clear_and_map(mem_region_t *reg, page_dir_t target_page_dir);
error_t mem_region_allocate_copy_and_map(mem_region_t *reg, page_dir_t target_page_dir, uintptr_t source_address);
error_t mem_region_allocate_fill_and_map(mem_region_t *reg, page_dir_t target_page_dir, page_filler_t *filler);
error_t mem_region_unmap_and_release(mem_region_t *reg, page_dir_t page_dir);

