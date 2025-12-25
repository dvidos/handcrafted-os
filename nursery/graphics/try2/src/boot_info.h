#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

typedef struct framebuffer_info {
    uint32_t fb_addr;  // physical address
    uint32_t width;
    uint32_t height;
    uint32_t pitch;    // bytes per row
    uint32_t bpp;      // bits per pixel
} framebuffer_info_t;

typedef struct __attribute__((packed)) e820_memory_entry {
    uint64_t base;
    uint64_t length;
    #define MEMORY_TYPE_AVAILABLE              1
    #define MEMORY_TYPE_RESERVED               2
    #define MEMORY_TYPE_ACPI_RECLAIMABLE       3
    #define MEMORY_TYPE_NVS                    4
    #define MEMORY_TYPE_BADRAM                 5
    uint32_t type;
    uint32_t acpi_ext; // optional, if ECX >= 24
} e820_memory_entry;

typedef struct memory_info {
    e820_memory_entry entries[32];
    uint32_t count;
} memory_info_t;

typedef struct boot_info {
    framebuffer_info_t fb;
    memory_info_t mem;

    char cmdline[128];

} boot_info_t;

#endif
