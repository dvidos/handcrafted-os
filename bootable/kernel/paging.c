#include <stdint.h>
#include <stdbool.h>

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
    the size of directory + pages is -> 4MB + 4KB,
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
*/


// remember all page directories and tables must be aligned to 4ks
// this also means that their addresses will have all 11 first bits as zero
uint32_t page_directory[1024] __attribute__((aligned(4096)));

uint32_t directory_entry_value(
    uint32_t address,
    bool cache_disable,
    bool write_through,
    bool user_access,
    bool write_enabled,
    bool page_present
);
uint32_t table_entry_value(
    uint32_t address,
    bool global,
    bool page_attr_table,
    bool cache_disable,
    bool write_through,
    bool user_access,
    bool write_enabled,
    bool page_present
);


void init_paging() {
    // we will also setup as many page entry tables as needed
    // then we must support acquiring and returning pages
    // we will setup c3 with the base page directory.
    // maybe we will swap it with every context switch in the sceduler,
    // or maybe we handle the page fault trap, to bring the page... from where?

    // This sets the following flags to the pages:
    //   Supervisor: Only kernel-mode can access them
    //   Write Enabled: It can be both read from and written to
    //   Not Present: The page table is not present

    uint32_t default_value = directory_entry_value(0, false, false, false, true, false);
    for (int i = 0; i < 1024; i++)
        page_directory[i] = default_value;

    // Enabling paging is actually very simple. All that is needed is to load CR3 with the address of the page directory and to set the paging (PG) and protection (PE) bits of CR0.
     // mov eax, page_directory
     // mov cr3, eax
     //
     // mov eax, cr0
     // or eax, 0x80000001
     // mov cr0, eax
}

inline uint32_t directory_entry_value(
    uint32_t address,
    bool cache_disable,
    bool write_through,
    bool user_access,
    bool write_enabled,
    bool page_present
) {
    // this value for 4 KB page directory entries
    // accessed set to zero,
    // page size set to zero for 4 KB
    // other bits left to zero
    return
        (address & 0xFFFFF000) << 12 |
        ((cache_disable & 0x01) << 4) |
        ((write_through & 0x01) << 3) |
        ((user_access   & 0x01) << 2) |
        ((write_enabled & 0x01) << 1) |
        (page_present   & 0x01);
}

inline uint32_t table_entry_value(
    uint32_t address,
    bool global,
    bool page_attr_table,
    bool cache_disable,
    bool write_through,
    bool user_access,
    bool write_enabled,
    bool page_present
) {
    // this value for 4 KB page directory entries
    // accessed set to zero,
    // page size set to zero for 4 KB
    // other bits left to zero
    return
        (address    & 0xFFFFF000) << 12 |
        ((global          & 0x01) << 8) |
        ((page_attr_table & 0x01) << 7) |
        ((cache_disable   & 0x01) << 4) |
        ((write_through   & 0x01) << 3) |
        ((user_access     & 0x01) << 2) |
        ((write_enabled   & 0x01) << 1) |
        (page_present     & 0x01);
}

inline bool is_page_accessed(void *page_address) {
    uint32_t *p = (uint32_t *)page_address;
    return (((*p) >> 5) & 0x01);
}

inline bool is_page_dirty(void *page_address) {
    uint32_t *p = (uint32_t *)page_address;
    return (((*p) >> 6) & 0x01);
}

// the idea would be, "Setup a memory space of 1MB for RW data, and a 250KB of code"
// I guess this when we load an app from disk...
