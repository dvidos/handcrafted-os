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

// ------------------------------------------------------------

static error_t _clear_filler_fill_page(size_t page_num, uintptr_t dest_addr, void *context) {
    memset((void *)dest_addr, 0, PAGE_SIZE);
    return OK;
}

error_t mem_region_allocate_clear_and_map(mem_region_t *reg, page_dir_t target_page_dir) {
    page_fill_t clear_filler = { .fill_page = _clear_filler_fill_page, .context = NULL };
    return mem_region_allocate_fill_and_map(reg, target_page_dir, &clear_filler);
}

static error_t _copy_filler_fill_page(size_t page_num, uintptr_t dest_addr, void *context) {
    uintptr_t base_source_address = (uintptr_t)context;
    uintptr_t source_address = base_source_address + page_num * PAGE_SIZE;
    
    memcpy((void *)dest_addr, (void *)source_address, PAGE_SIZE);
    return OK;
}

error_t mem_region_allocate_copy_and_map(mem_region_t *reg, page_dir_t target_page_dir, uintptr_t source_address) {
    page_fill_t copy_filler = { .fill_page = _copy_filler_fill_page, .context = (void *)source_address };
    return mem_region_allocate_fill_and_map(reg, target_page_dir, &copy_filler);
}

error_t mem_region_allocate_fill_and_map(mem_region_t *reg, page_dir_t target_page_dir, page_fill_t *filler) {
    page_dir_t curr_page_dir = vmm_get_page_directory_register();

    if (_util_page_addr == 0) return ERR_NOT_INITIALIZED;
    virt_addr_t working_page_address = _util_page_addr;

    int pages = BYTES_TO_PAGES(reg->size);
    for (int i = 0; i < pages; i++) {
        // first grab a new page
        phys_addr_t page_phys_addr = pmm_allocate_physical_page();
        if (page_phys_addr == 0)
            return ERR_NO_MEMORY;
        
        // temporarily map to where the kernel can copy pages
        vmm_map_virtual_to_physical(working_page_address, page_phys_addr, curr_page_dir, true, true);

        // fill this page (zero / copy / load from file / whatever)
        error_t err = filler->fill_page(i, working_page_address, filler->context);
        if (err) return err; // ideally roll back everything, we are leaking physical memory here

        // unmap from the previous mapping
        vmm_unmap(working_page_address, curr_page_dir);

        // map to the final virtual address, possibly protecting it.
        bool user_accessible = (reg->flags & REGION_USER_ACCESSIBLE);
        bool write_enable    = (reg->flags & REGION_WRITE_ENABLE);
        vmm_map_virtual_to_physical(reg->address + i * PAGE_SIZE, page_phys_addr, target_page_dir, user_accessible, write_enable);
    }

    return OK;
}

// --------------------------------------------------------------------

error_t mem_region_unmap_and_release(mem_region_t *reg, page_dir_t page_dir) {

    int pages = BYTES_TO_PAGES(reg->size);
    for (int i = 0; i < pages; i++) {
        virt_addr_t virt_page = reg->address + (i * PAGE_SIZE);
        phys_addr_t phys_page = vmm_resolve(virt_page, page_dir);

        // unmap from here, then release the page
        vmm_unmap(virt_page, page_dir);
        pmm_free_physical_page(phys_page);
    }

    return OK;
}

// ---------------------------------------------------------------------

const char *mem_region_usage_name(mem_region_t *reg) {
    if (reg->name) return reg->name;

    if      ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_CODE)  return "code";
    else if ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_DATA)  return "data";
    else if ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_STACK) return "stack";
    else if ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_HEAP)  return "heap";
    else if ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_MMIO)  return "mmio";
    else if ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_SHMEM) return "shmem";
    else if ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_FILE)  return "file";
    else if ((reg->flags & REGION_USAGE_MASK) == REGION_USAGE_GUARD) return "guard";

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