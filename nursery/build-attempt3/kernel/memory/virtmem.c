#include "../include/bits.h"
#include "physmem.h"
#include "virtmem.h"
#include "../utils/panic.h"
#include "../logger/logger.h"
#include "../arch/cpu.h"
#include "../klib/string.h"
#include "../memory/mem_region.h"

MODULE("VMEM", LOG_LEVEL_WARN);

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

// this to be included in every page directory we create,
// so that kernel structures and code are always available.
static struct {
    page_dir_t page_directory; 
    phys_addr_t start_address;
    phys_addr_t end_address;
    mem_map_t *kernel_phys_map;
} kernel_info;

page_dir_t vmm_create_page_directory(bool map_kernel_space);



static inline uint32_t create_directory_entry_value(uintptr_t address, bool cache_disable, bool write_through,
    bool user_access, bool write_enabled, bool page_present) {
    // this value for 4 KB page directory entries
    // accessed set to zero,
    // page size set to zero for 4 KB
    // other bits left to zero
    return
        (((uint32_t)address)      & 0xFFFFF000) | // no shifting here
        (((uint32_t)cache_disable & 0x01) << 4) |
        (((uint32_t)write_through & 0x01) << 3) |
        (((uint32_t)user_access   & 0x01) << 2) |
        (((uint32_t)write_enabled & 0x01) << 1) |
        (((uint32_t)page_present  & 0x01));
}

static inline uint32_t create_table_entry_value(uintptr_t address, bool global, bool page_attr_table,
    bool cache_disable, bool write_through, bool user_access, bool write_enabled, bool page_present) {
    // this value for 4 KB page directory entries
    // accessed set to zero,
    // page size set to zero for 4 KB
    // other bits left to zero
    return
        (((uint32_t)address)        & 0xFFFFF000) | // no shifting here
        (((uint32_t)global          & 0x01) << 8) |
        (((uint32_t)page_attr_table & 0x01) << 7) |
        (((uint32_t)cache_disable   & 0x01) << 4) |
        (((uint32_t)write_through   & 0x01) << 3) |
        (((uint32_t)user_access     & 0x01) << 2) |
        (((uint32_t)write_enabled   & 0x01) << 1) |
        (((uint32_t)page_present    & 0x01));
}

// common to both page directory and page tables
static inline bool is_entry_present(uintptr_t entry_value) {
    return (entry_value & 0x01);
}

// common to both page directory and page tables
static inline uintptr_t get_entry_address(uintptr_t entry_value) {
    return (entry_value & 0xFFFFF000);
}

// common to both page directory and page tables
static inline uint32_t get_table_entry(virt_addr_t table_address, uint32_t index) {
    return ((uint32_t *)table_address)[index];
}

// common to both page directory and page tables
static inline void set_table_entry(virt_addr_t table_address, uint32_t index, uint32_t value) {
    ((uint32_t *)table_address)[index] = value;
}

// extracts the page directory entry num from a virtual address
static inline uint32_t virt_addr_to_page_directory_index(virt_addr_t virtual_address) {
    // highest 10 bits (31-22) are the entry of the page table
    return (((uint32_t)virtual_address) >> 22) & 0x3FF;
}

// extracts the page table entry num from a virtual address
static inline uint32_t virt_addr_to_page_table_index(virt_addr_t virtual_address) {
    // second 10 bits (21-12) are the entry of the page table
    return (((uint32_t)virtual_address) >> 12) & 0x3FF;
}

// extracts the physical page offset from a virtual address
static inline uint32_t virt_addr_to_physical_page_offset(virt_addr_t virtual_address) {
    // the lowest 12 (11-0) bits are offset into a 4KB space
    return ((uint32_t)virtual_address) & 0xFFF;
}

// --------------------------------------------------------

void vmm_initialize(phys_addr_t kernel_start_address, phys_addr_t kernel_end_address, mem_map_t *kernel_phys_map) {
    
    // TODO: here, accept a full memory map, and keep it referenced, as it will be useful as hell.

    kernel_info.start_address = kernel_start_address;
    kernel_info.end_address = kernel_end_address;

    // create a page directory for kernel.
    kernel_info.page_directory = vmm_create_page_directory(true);

    // log_debug("Kernel page directory contents:");
    // log_debug_hex(kernel_page_direcory, 4096, (uint32_t)kernel_page_direcory);

    // void *pt = get_entry_address(get_table_entry(kernel_page_direcory, 0));
    // log_debug("Kernel first page table contents:");
    // log_debug_hex(pt, 4096, (uint32_t)pt);

    // void *va = (void *)(1024*1024 + 4096 + 7); // 1 MB
    // void *pa = vmm_resolve(va, kernel_page_direcory);
    // log_debug("Virtual address 0x%p resolves to physical address 0x%p", va, pa);

    // now enable paging (fingers crossed!)
    vmm_set_page_directory_register(kernel_info.page_directory);
    vmm_enable_paging();

    log_debug("Virtual memory paging initialized, range 0x%x - 0x%x will always be identity mapped");
}

phys_addr_t vmm_resolve(virt_addr_t virtual_addr, page_dir_t page_dir_addr) {
    // For each virtual address, when we are dealing with 4K pages:
    //     10 bits 31-22 dictate the page directory entry (we find the table)
    //     10 bits 21-12 dictate the page table entry (we find the page)
    //     12 bits 11-0  dictate the byte within the page (12 bits address a 4KB space)
    uint32_t index, entry;
    phys_addr_t address;

    // first resolve page directory.
    index = virt_addr_to_page_directory_index(virtual_addr);
    entry = get_table_entry(page_dir_addr, index);
    if (!is_entry_present(entry))
        return 0;
    address = get_entry_address(entry);
    if (address == 0)
        return 0;
    
    // then resolve page table
    index = virt_addr_to_page_table_index(virtual_addr);
    entry = get_table_entry(address, index);
    if (!is_entry_present(entry))
        return 0;
    address = get_entry_address(entry);
    if (address == 0)
        return 0;
    
    // now resolve final address
    uint32_t offset = virt_addr_to_physical_page_offset(virtual_addr);
    return (address + offset);
}

// map the virtual address to resolve to the physical one for the particular page directory.
void vmm_map_virtual_to_physical(virt_addr_t virtual_addr, phys_addr_t physical_addr, page_dir_t page_dir, bool user_accessible, bool write_enable) {
    log_trace("Mapping phys addr 0x%x to virt addr 0x%x, page dir 0x%x", physical_addr, virtual_addr, page_dir);

    uint32_t page_dir_index = virt_addr_to_page_directory_index(virtual_addr);
    uint32_t page_dir_entry = get_table_entry(page_dir, page_dir_index);
    uintptr_t page_table_address;
    // log_debug("pd address = 0x%p, pd index = %d, pd entry = 0x%x", page_dir_addr, page_dir_index, page_dir_entry);

    if (is_entry_present(page_dir_entry)) {
        page_table_address = get_entry_address(page_dir_entry);
    } else {
        // we need to create one
        page_table_address = pmm_allocate_physical_page();
        log_debug("Allocated new physical page at 0x%p for new page table", page_table_address);
        memset((void *)page_table_address, 0, 4096);
        uint32_t page_dir_value = create_directory_entry_value(
            page_table_address,
            true, // cache disable
            true, // write through
            true, // user accessible
            true, // write enabled
            true  // page present
        );
        // log_debug("new page_dir entry value = 0x%08x", page_dir_value);
        set_table_entry(page_dir, page_dir_index, page_dir_value);
    }

    // now map the physical page in the page_table
    uint32_t page_table_index = virt_addr_to_page_table_index(virtual_addr);
    uint32_t page_table_entry = get_table_entry(page_table_address, page_table_index);
    if (is_entry_present(page_table_entry))
        return; // already mapped
    
    page_table_entry = create_table_entry_value(
        physical_addr,
        true, // global
        true, // PAT
        true, // cache disable
        true, // write through
        user_accessible, // user accessible
        write_enable, // writable
        true  // page present
    );
    // log_debug("new page_table entry value = 0x%08x", page_table_entry);
    set_table_entry(page_table_address, page_table_index, page_table_entry);
}

// unmap the virtual address to resolve to the physical one for the particular page directory.
void vmm_unmap(virt_addr_t virtual_addr, page_dir_t page_dir_addr) {
    // clear the entry of the page table, if all the page table is clear, 
    // maybe remove the entry from the page directory and free the page.

    log_trace("Unmapping virt addr 0x%x, page dir 0x%x", virtual_addr, page_dir_addr);

    // find page table address
    uint32_t page_dir_index = virt_addr_to_page_directory_index(virtual_addr);
    uint32_t page_dir_entry = get_table_entry(page_dir_addr, page_dir_index);
    if (!is_entry_present(page_dir_entry))
        return; // no need, there's no page_table at all
    uintptr_t page_table_address = get_entry_address(page_dir_entry);
    
    // clear the page table entry
    uint32_t page_table_index = virt_addr_to_page_table_index(virtual_addr);
    set_table_entry(page_table_address, page_table_index, 0);

    // see if the whole page table is empty, to maybe free it.
    bool page_table_completely_empty = true;
    for (int i = 0; i < 1024; i++) {
        if (((uint32_t *)page_table_address)[i] != 0) {
            page_table_completely_empty = false;
            break;
        }
    }
    if (page_table_completely_empty) {
        log_debug("Page table is clear, freeing physical page at 0x%x", page_table_address);
        pmm_free_physical_page((phys_addr_t)page_table_address);
        // remove entry from page directory
        set_table_entry(page_dir_addr, page_dir_index, 0);
    }
}

// map a range to itself
void vmm_identity_map_range(phys_addr_t start_addr, phys_addr_t end_addr, page_dir_t page_dir_addr) {
    log_trace("Identity mapping range 0x%p - 0x%p, page_dir=0x%p", start_addr, end_addr, page_dir_addr);
    for (phys_addr_t addr = start_addr; addr <= end_addr; addr += 4096) {
        vmm_map_virtual_to_physical(addr, addr, page_dir_addr, true, true);
    }
}


// Enabling paging is actually very simple. All that is needed is 
// to load CR3 with the address of the page directory 
// and to set the paging (PG) and protection (PE) bits of CR0.
void vmm_set_page_directory_register(page_dir_t value) {
    log_trace("Setting CR3 to 0x%x", value);

    __asm__ __volatile__(
        "mov %0, %%eax\n\t"
        "mov %%eax, %%cr3\n\t"
        :             // no output values
        : "g"(value)  // input No 0
        : "eax"       // garbled registers
    );
}

// get current directory register (cr3)
page_dir_t vmm_get_page_directory_register() {
    page_dir_t value;

    __asm__ __volatile__(
        "movl %%cr3, %0\n\t"
        : "=g"(value)  // output No 0
        :              // no input values
        :              // garbled registers
    );

    return value;
}

inline void vmm_invalidate_cached_address(virt_addr_t virtual_addr) {
    // supported on i486+
    asm volatile("invlpg (%0)" : : "r" (virtual_addr) : "memory");
}

void vmm_enable_paging() {
    log_trace("Enabling memory paging in CPU");

    __asm__ __volatile__(
        "mov %%cr0, %%eax\n\t"
        "or $0x80000000, %%eax\n\t"  // turn on bit 31
        "mov %%eax, %%cr0"
        :       // no outputs
        :       // no inputs
        : "eax" // garbled registers
    );
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


page_dir_t vmm_get_kernel_page_directory() {
    return kernel_info.page_directory;
}

// handles page faults. 
// see https://wiki.osdev.org/Exceptions#Page_Fault
void vmm_page_fault_handler(uint32_t error_code) {
    // CR2 contains the virtual address that caused the error.
    bool page_present    = IS_BIT(error_code, 0);
    bool write_attempt   = IS_BIT(error_code, 1);
    bool supervisor_code = IS_BIT(error_code, 2);

    uint32_t memory_address = 0;
    __asm__ __volatile__("mov %%cr2, %0" : "=g"(memory_address));

    page_dir_t page_dir_address = 0;
    __asm__ __volatile__("mov %%cr3, %0" : "=g"(page_dir_address));

    log_warn("Page fault, %s %s page by %s process, at 0x%x, page dir at 0x%p",
        write_attempt ? "writing on" : "reading a",
        page_present ? "protected" : "missing",
        supervisor_code ? "supervisor" : "user",
        memory_address,
        page_dir_address
    );

    // solution for now is to identity map this, just for fun
    // but, if we had a memory map (the mem_regions), we could identify who errored
    // e.g. stack underflow, or heap overflow, guard, mem-mapped file, etc

    memory_address = ROUND_DOWN_4K(memory_address);
    vmm_map_virtual_to_physical(memory_address, memory_address, page_dir_address, true, true);
}



// allocates and creates a new page directory
page_dir_t vmm_create_page_directory(bool map_kernel_space) {
    page_dir_t page_dir = pmm_allocate_physical_page();
    if (page_dir == INVALID_PAGE) {
        panic("Failed to allocate physical page for page directory!");
    }
    memset((void *)page_dir, 0, PAGE_SIZE);

    if (map_kernel_space) {
        // the kernel (code, data, heap etc) must be mapped in the same address in all address spaces.
        // that way, we can switch CR3 and jump into an elf loading function without issues.
        // or execute kerel code, or keep variables and pointers sane when switching tasks
        // TODO: it seems VMM needs to know a lot about kernel mem regions...
        vmm_identity_map_range(kernel_info.start_address, kernel_info.end_address, page_dir);
    }

    log_trace("vmm_create_page_directory() -> 0x%p", page_dir);
    return page_dir;
}

// allocates pages and maps them to the virtual addresses requested (end address is non-inclusive)
void vmm_allocate_memory_range(virt_addr_t virt_addr_start, virt_addr_t virt_addr_end, page_dir_t page_dir_addr) {
    log_trace("vmm_allocate_memory_range(0x%p - 0x%p, PD=0x%p)", virt_addr_start, virt_addr_end, page_dir_addr);

    // TODO: this should update the memory map of the kernel/process
    for (virt_addr_t virt_addr = virt_addr_start; virt_addr < virt_addr_end; virt_addr += 4096) {
        phys_addr_t phys_page_addr = pmm_allocate_physical_page();
        vmm_map_virtual_to_physical(virt_addr, phys_page_addr, page_dir_addr, true, true);
    }
}

// frees any pointed pages, page tables, and the page directory itself
void vmm_destroy_page_directory(page_dir_t page_dir_address) {
    log_trace("vmm_destroy_page_directory(0x%x)", page_dir_address);
    pushcli();

    // free linked tables and pages 
    uint32_t entry;
    for (int pd_index = 0; pd_index < 1024; pd_index++) {
        entry = get_table_entry(page_dir_address, pd_index);
        if (!is_entry_present(entry))
            continue;
        
        uintptr_t page_table_address = get_entry_address(entry);
        if (page_table_address == 0)
            continue;

        // free any linked physical pages first
        for (int pt_index = 0; pt_index < 1024; pt_index++) {
            entry = get_table_entry(page_table_address, pt_index);
            if (!is_entry_present(entry))
                continue;

            phys_addr_t phys_page_address = get_entry_address(entry);
            if (phys_page_address == 0)
                continue;

            // we only free our extra pages, not the kernel ones.
            if (phys_page_address >= kernel_info.start_address && phys_page_address <= kernel_info.end_address)
                continue;

            pmm_free_physical_page((phys_addr_t)phys_page_address);
        }

        pmm_free_physical_page((phys_addr_t)page_table_address);
    }

    // we can now free the page directory itself
    pmm_free_physical_page((phys_addr_t)page_dir_address);
    popcli();
}

static void _dump_page_directory_print(uint32_t virt_mem_group_start, uint32_t virt_mem_group_end, uint32_t phys_mem_group_start, uint32_t phys_mem_group_end) {

    if (virt_mem_group_start == virt_mem_group_end) {
        // single mapping
        log_debug("Virt 0x%05xxxx             --> Phys 0x%05xxxx           %s", 
            virt_mem_group_start >> 12, 
            phys_mem_group_start >> 12,
            virt_mem_group_start == phys_mem_group_start ? "(identity)" : ""
        );
    } else {
        // group mapping
        log_debug("Virt 0x%05xxxx..0x%05xxxx --> Phys 0x%05xxxx..0x%05xxxx  %d KB  %s", 
            virt_mem_group_start >> 12, 
            virt_mem_group_end   >> 12, 
            phys_mem_group_start >> 12,
            phys_mem_group_end   >> 12,
            (virt_mem_group_end - virt_mem_group_start) / 1024,
            virt_mem_group_start == phys_mem_group_start && virt_mem_group_end == phys_mem_group_end ? "(identity)" : ""
        );
    }
}

static void _dump_page_directory_aggregate(int call, uint32_t virt_addr, uint32_t phys_addr) {
    // we'll try to group the entries
    static uint32_t virt_mem_group_start;
    static uint32_t virt_mem_group_end;
    static uint32_t phys_mem_group_start;
    static uint32_t phys_mem_group_end;
    static bool in_group;

    if (call == 1) { // we are initializing
        virt_mem_group_start = 0;
        virt_mem_group_end = 0;
        phys_mem_group_start = 0;
        phys_mem_group_end = 0;
        in_group = false;

    } else if (call == 2) { // found a valid mapping
        if (in_group) {
            // see if we are just extending it, or there is a gap
            if (virt_addr == virt_mem_group_end + 4096 && phys_addr == phys_mem_group_end + 4096
            ) {
                // same group, extend
                virt_mem_group_end += 4096;
                phys_mem_group_end += 4096;
            } else {
                // different group, print, restart
                _dump_page_directory_print(virt_mem_group_start, virt_mem_group_end, phys_mem_group_start, phys_mem_group_end);
                virt_mem_group_start = virt_addr;
                virt_mem_group_end = virt_addr;
                phys_mem_group_start = phys_addr;
                phys_mem_group_end = phys_addr;
            }
        } else {
            // we were not in a group, we can start one
            virt_mem_group_start = virt_addr;
            virt_mem_group_end = virt_addr;
            phys_mem_group_start = phys_addr;
            phys_mem_group_end = phys_addr;
            in_group = true;
        }
    } else if (call == 3) { // finished with all pages
        if (in_group) {
            // show the last group
            _dump_page_directory_print(virt_mem_group_start, virt_mem_group_end, phys_mem_group_start, phys_mem_group_end);
        }
    }
}

void vmm_dump_page_directory(virt_addr_t page_dir_address) {
    // essentially, map the mapping that a page directory has.
    // try to group common areas together.
    log_debug("Page directory at 0x%x mapping", page_dir_address);

    uint32_t entry;

    // free linked tables and pages 
    _dump_page_directory_aggregate(1, 0, 0);
    for (int pd_index = 0; pd_index < 1024; pd_index++) {
        entry = get_table_entry(page_dir_address, pd_index);
        if (!is_entry_present(entry))
            continue;
        
        uintptr_t page_table_address = get_entry_address(entry);
        if (page_table_address == 0)
            continue;

        // free any linked physical pages first
        for (int pt_index = 0; pt_index < 1024; pt_index++) {
            entry = get_table_entry(page_table_address, pt_index);
            if (!is_entry_present(entry))
                continue;

            uint32_t physical_address = (uint32_t)get_entry_address(entry);
            if (physical_address == 0)
                continue;
            
            uint32_t virtual_address = SET_BIT_RANGE(pd_index, 31, 22) | SET_BIT_RANGE(pt_index, 21, 12);
            _dump_page_directory_aggregate(2, virtual_address, physical_address);
        }
    }
    _dump_page_directory_aggregate(3, 0, 0);
}
