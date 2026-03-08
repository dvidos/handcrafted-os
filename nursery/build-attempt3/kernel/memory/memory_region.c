#include "memory_region.h"
#include "../klib/string.h"
#include "../memory/virtmem.h"
#include "../memory/physmem.h"


static phys_addr_t _util_page_addr = 0;

void mem_region_set_util_page_address(phys_addr_t addr) {
    _util_page_addr = addr;
}

// ------------------------------------------------------------

memory_region_t mem_region_empty() {
    return (memory_region_t){
        .address = 0,
        .size = 0,
        .flags = 0,
        .name = 0
    };
}

bool mem_region_is_empty(memory_region_t *reg) {
    if (reg == NULL) return true;
    return (reg->address == 0 && reg->size == 0 && reg->flags == 0 && reg->name == 0);
}

// ------------------------------------------------------------

static error_t _clear_filler_fill_page(size_t page_num, uintptr_t dest_addr, void *context) {
    memset((void *)dest_addr, 0, PAGE_SIZE);
    return OK;
}

error_t mem_region_allocate_clear_and_map(memory_region_t *reg, page_dir_t target_page_dir) {
    page_fill_t clear_filler = { .fill_page = _clear_filler_fill_page, .context = NULL };
    return mem_region_allocate_fill_and_map(reg, target_page_dir, &clear_filler);
}

// ------------------------------------------------------------

static error_t _copy_filler_fill_page(size_t page_num, uintptr_t dest_addr, void *context) {
    uintptr_t base_source_address = (uintptr_t)context;
    uintptr_t source_address = base_source_address + page_num * PAGE_SIZE;
    
    memcpy((void *)dest_addr, (void *)source_address, PAGE_SIZE);
    return OK;
}

error_t mem_region_allocate_copy_and_map(memory_region_t *reg, page_dir_t target_page_dir, uintptr_t source_address) {
    page_fill_t copy_filler = { .fill_page = _copy_filler_fill_page, .context = (void *)source_address };
    return mem_region_allocate_fill_and_map(reg, target_page_dir, &copy_filler);
}

// -------------------------------------------------------------

error_t mem_region_allocate_fill_and_map(memory_region_t *reg, page_dir_t target_page_dir, page_fill_t *filler) {
    page_dir_t curr_page_dir = vmm_get_page_directory_register();

    if (_util_page_addr == 0) return ERR_NOT_INITIALIZED;
    virt_addr_t working_page_address = _util_page_addr;

    int pages = BYTES_TO_PAGES(reg->size);
    for (int i = 0; i < pages; i++) {
        // first grab a new page
        phys_addr_t page_phys_addr = pmm.allocate_physical_page();
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

error_t mem_region_unmap_and_release(memory_region_t *reg, page_dir_t page_dir) {

    int pages = BYTES_TO_PAGES(reg->size);
    for (int i = 0; i < pages; i++) {
        virt_addr_t virt_page = reg->address + (i * PAGE_SIZE);
        phys_addr_t phys_page = vmm_resolve(virt_page, page_dir);

        // unmap from here, then release the page
        vmm_unmap(virt_page, page_dir);
        pmm.free_physical_page(phys_page);
    }

    return OK;
}
