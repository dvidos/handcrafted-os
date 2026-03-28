#include "vmm_internal.h"


// Enabling paging is actually very simple. All that is needed is 
// to load CR3 with the address of the page directory 
// and to set the paging (PG) and protection (PE) bits of CR0.
void vmm_set_page_directory_register(page_dir_t value) {
    log_trace("vmm_set_page_directory_register(0x%x)", value);

    __asm__ __volatile__(
        "mov %0, %%eax\n\t"
        "mov %%eax, %%cr3\n\t"
        :             // no output values
        : "g"(value)  // input No 0
        : "eax"       // garbled registers
    );
}

// get current directory register (cr3)
page_dir_t vmm_get_current_page_dir() {
    page_dir_t value;

    __asm__ __volatile__(
        "movl %%cr3, %0\n\t"
        : "=g"(value)  // output No 0
        :              // no input values
        :              // garbled registers
    );

    return value;
}

void vmm_invalidate_cached_address(virt_addr_t virtual_addr) {
    // supported on i486+
    asm volatile("invlpg (%0)" : : "r" (virtual_addr) : "memory");
}

void vmm_enable_paging() {
    log_trace("Enabling memory paging in CPU");

    // enable big pages support
    uint32_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 4);        // CR4.PSE
    asm volatile("mov %0, %%cr4" :: "r"(cr4));    


    __asm__ __volatile__(
        "mov %%cr0, %%eax\n\t"
        "or $0x80000000, %%eax\n\t"  // turn on bit 31
        "mov %%eax, %%cr0"
        :       // no outputs
        :       // no inputs
        : "eax" // garbled registers
    );

    kinfo.paging_enabled = true;
}

void vmm_disable_paging() {
    log_trace("Disabling memory paging in CPU");

    __asm__ __volatile__(
        "mov %%cr0, %%eax\n\t"
        "and $0x7FFFFFFF, %%eax\n\t"  // turn off bit 31
        "mov %%eax, %%cr0"
        :       // no outputs
        :       // no inputs
        : "eax" // garbled registers
    );
}

