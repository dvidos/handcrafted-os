#include "mem_region.h"
#include "../klib/string.h"
#include "../memory/virtmem.h"
#include "../memory/physmem.h"
#include "../logger/logger.h"
#include "../include/va_list.h"

MODULE("MEM_MAP", LOG_LEVEL_WARN);


static phys_addr_t _util_page_addr = 0;

void mem_region_set_mappable_page_address(phys_addr_t addr) {
    _util_page_addr = addr;
}
phys_addr_t mem_region_get_mappable_page_address() {
    return _util_page_addr;
}

// ------------------------------------------------------------

const char *mem_region_usage_name(mem_region_t *reg) {
    if (reg->name[0] != 0)
        return reg->name;
    
    if      ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_CODE)  return "code";
    else if ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_DATA)  return "data";
    else if ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_STACK) return "stack";
    else if ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_HEAP)  return "heap";
    else if ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_MMIO)  return "mmio";
    else if ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_SHMEM) return "shmem";
    else if ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_FILE)  return "file";
    else if ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_GUARD) return "guard";
    else if ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_ELF)   return "elf";

    return "other";
}

void mem_region_describe_flags(mem_region_t *reg, char *buffer) {
    buffer[0] = 0;

    strcat(buffer, reg->flags & REGION_WRITE_ENABLE ? "write" : "ro");
    strcat(buffer, ",");
    strcat(buffer, reg->flags & REGION_USER_ACCESSIBLE ? "user" : "kernel");
}

void mem_region_formatter(log_write_stream_t *stream, va_list args) {
    mem_region_t *reg = va_arg(args, mem_region_t *);
    char flags[64];
    
    mem_region_describe_flags(reg, flags);
    stream->printf(stream->context, "addr=0x%x, size=%x/%uKB, flags=%-16s, usage=%s",
        reg->address,
        reg->size,
        reg->size / 1024,
        flags,
        mem_region_usage_name(reg)
    );
}

bool mem_region_contains_address(mem_region_t *reg, uintptr_t address) {
    return address >= reg->address && address < reg->address + reg->size;
}