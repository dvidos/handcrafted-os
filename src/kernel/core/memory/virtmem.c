#include <stdint.h>
#include <stdbool.h>
#include <bits.h>
#include <memory/physmem.h>
#include <drivers/screen.h>
#include <klog.h>
#include <klib/string.h>

/*
   Paging is mapping a virtual address to a physical one.
   On hardware, this is done by having a cache between the CPU
   and the actual address bus.
   x86 supports paging with page sizes of 4MB, 2MB and 4KB.
   the cr3 register should point to a 4 KB table called the "Page Directory"
   that table contains 1024 entries of 32bits each.
   each entry is pointing to the physical address of a 4 KB table called "Page Table"
   that table contais 1024 entries of 32 bits each.
   each entry is pointing to the phyisical address of a 4 KB memory page...

   to map the whole 4 GB space, we need:
        4 * 1024 * 1024 * 1024 = 4294967296
        we can divide this into 1 million of 4KB pages
        to support them we need 1K page tables
        to support all those page tables we need one page directory.
    the size of directory + pages is -> 4KB + 4MB,
    but maybe we don't need to create them all

   For each virtual address, when we are dealing with 4K pages:
        10 bits 31-22 dictate the page directory entry (we find the table)
        10 bits 21-12 dictate the page table entry (we find the page)
        12 bits 11-0  dictate the byte within the page (12 bits address a 4KB space)

   Now, the OS is supposed to maintain different page directories and page tables
   for each application, therefore, even if two applications are looking at the same
   virtual address, we can make them see the physical pages that we have allocated for them.

   When a virtual address cannot be mapped to a physical one,
   a fault will be raised (interrupt or trap) for the OS
   to provide the correct paging.

   Notice that we don't have to use this system, but it's helpful.
   It's helpful because we can map any address the app sees
   into any physical area we have reserved for the application specifically.
   That means, if this app crashes, it can corrupt only its own memory,
   not the memory of other apps.

   Note that kernels usually "identity map" the base 640k so that
   the virtual and the physical address always match.
   This helps with video memory, for example. (https://wiki.osdev.org/Identity_Paging)

   See also https://wiki.osdev.org/Paging
   Don't forget, paging is just the second level after selectors (GDT / LDT)

   If we want a bit to represent a flag about each 4 KB page in the 4 GB space,
   we will need 32768 32-bit words

   Usage:
   1. isolation of processes and ability to see the full 4GB even when physical memory is less
   2. ability for processes to see the same page, even if their virtual addresses differ
   3. ability for instances of programs to share code and RO data pages, for memory economy
                   also libraries code can be shared in this way
   4. effective way to communicate between programs, to share a writable page
   4. virtual memory swapping (with opportunities for prefetching, proactively storing to disk,
                   even presenting larger virtual space than physically available)
   5. protection - pages can be marked read only, a page fault is generated when attempted to write to
   6. protection - pages can be supervisor or user, a page fault is generated when accessed by unsuprvised process
                   (supervisor is anything above application level 3 on the Current Privilege Level of the segment descriptor)
   7. mapping of files or device constants, without loading them, and only loading what's actually used,
                   based on which page faults have been fired.

   In conclusion though, I think the only real drive for paging is swapping...

   Apparently, the kernel needs to allocate physical memory for a processes
   When it does that, it applies paging, to map the process' virtual address
   space to the physical memory it has "allocated". To know what is allocated
   and what not, the kernel has to have some housekeeping method, e.g.
   a linked list of free pages or something similar. If it keeps a list
   of free pages, it can take some out of there and put them in the list
   of allocated pages for a process or for the kernel itself.

   Essentially, we ask the paging allocator to allocate a page for us,
   (it can be any physical address)
   and we give it the virtual address space we want it to be mapped to.
   Upon exiting the process, we free the page, back to the unallocated list.

   Oh, and after spending half an hour at crafting beautiful structs with unions,
   it turns out the bit allocation is implementation specific and never guaranteed!
*/



static inline uint32_t create_directory_entry_value(void *address, bool cache_disable, bool write_through,
    bool user_access, bool write_enabled, bool page_present) {
    // this value for 4 KB page directory entries
    // accessed set to zero,
    // page size set to zero for 4 KB
    // other bits left to zero
    return
        (((uint32_t)address) & 0xFFFFF000) << 12 |
        ((cache_disable & 0x01) << 4) |
        ((write_through & 0x01) << 3) |
        ((user_access   & 0x01) << 2) |
        ((write_enabled & 0x01) << 1) |
        (page_present   & 0x01);
}

static inline uint32_t create_table_entry_value(void *address, bool global, bool page_attr_table,
    bool cache_disable, bool write_through, bool user_access, bool write_enabled, bool page_present) {
    // this value for 4 KB page directory entries
    // accessed set to zero,
    // page size set to zero for 4 KB
    // other bits left to zero
    return
        (((uint32_t)address) & 0xFFFFF000) << 12 |
        ((global          & 0x01) << 8) |
        ((page_attr_table & 0x01) << 7) |
        ((cache_disable   & 0x01) << 4) |
        ((write_through   & 0x01) << 3) |
        ((user_access     & 0x01) << 2) |
        ((write_enabled   & 0x01) << 1) |
        (page_present     & 0x01);
}

static inline uint32_t *get_page_table_address(void *page_dir_addr, uint32_t index) {
    // least significant 12 bits are for flags
    return (uint32_t *)(((uint32_t *)page_dir_addr)[index] & 0xFFFFF000);
}

static inline uint32_t *get_phys_page_address(void *page_table_addr, uint32_t index) {
    // least significant 12 bits are for flags
    return (uint32_t *)(((uint32_t *)page_table_addr)[index] & 0xFFFFF000);
}

static inline void set_page_dir_entry(void *page_dir_addr, uint32_t index, uint32_t value) {
    ((uint32_t *)page_dir_addr)[index] = value;
}

static inline void set_page_table_entry(void *page_table_addr, uint32_t index, uint32_t value) {
    ((uint32_t *)page_table_addr)[index] = value;
}

static inline uint32_t get_page_directory_index(void *virtual_address) {
    // highest 10 bits (31-22) are the entry of the page table
    return (((uint32_t)virtual_address) >> 22) & 0x3FF;
}

static inline uint32_t get_page_table_index(void *virtual_address) {
    // second 10 bits (21-12) are the entry of the page table
    return (((uint32_t)virtual_address) >> 12) & 0x3FF;
}

static inline uint32_t get_physical_page_offset(void *virtual_address) {
    // the lowest 12 (11-0) bits are offset into a 4KB space
    return ((uint32_t)virtual_address) & 0xFFF;
}

void *resolve_virtual_address_to_physical_one(void *virtual_addr, void *page_dir_addr) {
    // For each virtual address, when we are dealing with 4K pages:
    //     10 bits 31-22 dictate the page directory entry (we find the table)
    //     10 bits 21-12 dictate the page table entry (we find the page)
    //     12 bits 11-0  dictate the byte within the page (12 bits address a 4KB space)

    uint32_t page_dir_index = get_page_directory_index(virtual_addr);
    void *page_table_addr = get_page_table_address(page_dir_addr, page_dir_index);
    if (page_table_addr == NULL)
        return NULL;
    
    uint32_t page_table_index = get_page_table_index(virtual_addr);
    void *phys_page_addr = get_phys_page_address(page_table_addr, page_table_index);
    if (phys_page_addr == NULL)
        return NULL;
    
    uint32_t offset_in_page = get_physical_page_offset(virtual_addr);
    return (void *)(phys_page_addr + offset_in_page);
}

// map the virtual address to resolve to the physical one for the particular page directory.
void map_virtual_address_to_physical(void *virtual_addr, void *physical_addr, void *page_dir_addr) {
    klog_trace("Mapping phys addr 0x%x to virt addr 0x%x, page dir 0x%x", physical_addr, virtual_addr, page_dir_addr);

    uint32_t page_dir_index = get_page_directory_index(virtual_addr);
    void *page_table_addr = get_page_table_address(page_dir_addr, page_dir_index);
    if (page_table_addr == NULL) {
        // we need to create one
        page_table_addr = allocate_physical_page();
        klog_debug("Allocated new physical page at 0x%x for new page table", page_table_addr);
        memset(page_table_addr, 0, 4096);
        set_page_dir_entry(page_dir_addr, page_dir_index, create_directory_entry_value(
            page_table_addr,
            true,
            true,
            true,
            true,
            true
        ));
    }

    // now map the physical page in the page_table
    uint32_t page_table_index = get_page_table_index(virtual_addr);
    set_page_table_entry(page_table_addr, page_table_index, create_table_entry_value(
        physical_addr,
        true,
        true,
        true,
        true,
        true,
        true,
        true
    ));
}

// unmap the virtual address to resolve to the physical one for the particular page directory.
void unmap_virtual_address(void *virtual_addr, void *page_dir_addr) {
    // clear the entry of the page table, if all the page table is clear, 
    // maybe remove the entry from the page directory and free the page.

    klog_trace("Unmapping virt addr 0x%x, page dir 0x%x", virtual_addr, page_dir_addr);

    uint32_t page_dir_index = get_page_directory_index(virtual_addr);
    uint32_t *page_table_addr = get_page_table_address(page_dir_addr, page_dir_index);
    if (page_table_addr == NULL)
        return; // no need, there's no page_table at all
    
    // clear the entry
    uint32_t page_table_index = get_page_table_index(virtual_addr);
    set_page_table_entry(page_table_addr, page_table_index, 0);

    // see if the whole page table is empty, to maybe free it.
    bool page_table_completely_empty = true;
    for (int i = 0; i < 1024; i++) {
        if (page_table_addr[i] != 0) {
            page_table_completely_empty = false;
            break;
        }
    }
    if (page_table_completely_empty) {
        klog_debug("Page table is clear, freeing physical page at 0x%x", page_table_addr);
        free_physical_page(page_table_addr);
        // remove entry from page directory
        set_page_dir_entry(page_dir_addr, page_dir_index, 0);
    }
}

// map a range to itself
void identity_map_range(void *start_addr, void *end_addr, void *page_dir_addr) {
    for (void *addr = start_addr; addr < end_addr; addr += 4096) {
        map_virtual_address_to_physical(addr, addr, page_dir_addr);
    }
}



// Enabling paging is actually very simple. All that is needed is 
// to load CR3 with the address of the page directory 
// and to set the paging (PG) and protection (PE) bits of CR0.
void set_page_directory_address_in_cpu(void *address) {
    __asm__ __volatile__(
        "mov %0, %%eax\n\t"
        "mov %%eax, %%cr3\n\t"
        :                         // no output values
        : "g"((uint32_t)address)  // input No 0
        : "eax"                   // garbled registers
    );
}

void enable_memory_paging_cpu_bit() {
    __asm__ __volatile__(
        "mov %%cr0, %%eax\n\t"
        "or $0x80000000, %%eax\n\t"  // turn on bit 31
        "mov %%eax, %%cr0"
        :       // no outputs
        :       // no inputs
        : "eax" // garbled registers
    );
}

void disable_memory_paging_cpu_bit() {
    __asm__ __volatile__(
        "mov %%cr0, %%eax\n\t"
        "and $0x7FFFFFFF, %%eax\n\t"  // turn off bit 31
        "mov %%eax, %%cr0"
        :       // no outputs
        :       // no inputs
        : "eax" // garbled registers
    );
}


void *kernel_page_direcory_address;

void init_virtual_memory_paging(void *kernel_start_address, void *kernel_end_address) {
    if (physical_page_size() != 4096)
        panic("Virtual memory supports 4KB pages only");
    
    // create a page directory for kernel.
    kernel_page_direcory_address = allocate_physical_page();
    memset(kernel_page_direcory_address, 0, 4096);
    identity_map_range(kernel_start_address, kernel_end_address, kernel_page_direcory_address);

    // now enable paging (fingers crossed!)
    set_page_directory_address_in_cpu(kernel_page_direcory_address);
    enable_memory_paging_cpu_bit();
}

