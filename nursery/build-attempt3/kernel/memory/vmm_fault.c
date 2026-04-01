#include "vmm_internal.h"


#define MAX_PAGE_FAULTS  3
static int page_fault_num = 0;

// handles page faults. 
// see https://wiki.osdev.org/Exceptions#Page_Fault
void vmm_page_fault_handler(trap_frame_t *tf) {

    page_fault_num++;
    if (page_fault_num > MAX_PAGE_FAULTS)
        panic("Too many page faults");

    uint32_t error_code = tf->err_code;
    uint32_t eip = tf->eip;

    uint32_t addr = 0;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(addr));

    page_dir_t cr3 = 0;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));

    char *mem_area = "";
    if      (mem_region_contains_address(&kmm.code, addr))          mem_area = "kernel code";
    else if (mem_region_contains_address(&kmm.data, addr))          mem_area = "kernel data";
    else if (mem_region_contains_address(&kmm.rodata, addr))        mem_area = "kernel rodata";
    else if (mem_region_contains_address(&kmm.bss, addr))           mem_area = "kernel bss";
    else if (mem_region_contains_address(&kmm.stack, addr))         mem_area = "kernel stack";
    else if (mem_region_contains_address(&kmm.mapping_pages, addr)) mem_area = "kernel mapping pages";
    else if (mem_region_contains_address(&kmm.pmm_bitmap, addr))    mem_area = "kernel pmm bmp";
    else if (mem_region_contains_address(&kmm.heap, addr))          mem_area = "kernel heap";
    else if (addr >= kmm.reserved_start && addr < kmm.reserved_end) mem_area = "kernel reserved area";
    else if (addr >= kmm.reserved_end)                              mem_area = "user memory space";

    log_warn("Page Fault (#%d):", page_fault_num);
    log_warn("   CS 0x%08x (0x%x is kernel, 0x%x is user)", tf->cs, KERNEL_CODE_SEGMENT, USER_CODE_SEGMENT);
    log_warn("  EIP 0x%08x (addr2line -f -e ./kernel/kernel.elf 0x%x)", eip, eip);
    log_warn("  CR3 0x%08x (0x%x is kernel)", cr3, kinfo.page_directory);
    log_warn("  CR2 0x%08x address space: %s", addr, mem_area);
    log_warn("  err 0x%08x", error_code);
    log_warn("      - P: %s", IS_BIT(error_code, 0) ? "page protection" : "page missing");
    log_warn("      - W: %s", IS_BIT(error_code, 1) ? "write attempt" : "read attempt");
    log_warn("      - U: %s", IS_BIT(error_code, 1) ? "ring 3 (user)" : "ring 0-2 (supervisor)");

    // log_debug_fmt(vmm_pagedir_log_formatter, "cr3:", cr3);
    // vmm_read_all_identity_addresses();

    // solution for now is to identity map this, just for fun
    // but, if we had a memory map (the mem_regions), we could identify who errored
    // e.g. stack underflow, or heap overflow, guard, mem-mapped file, etc
    if (addr >= kmm.reserved_start && addr < kmm.reserved_end) {
        log_warn("this is kernel space, will attempt identity mapping, but needs fixing");
        error_t err = rmw_map_page(vmm_round_down(addr), vmm_round_down(addr), false, true);
        if (err)
            panic("Failed to identity map: %d - %s", err, strerror(err));
    }
}

