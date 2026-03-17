#include "../include/macros.h"
#include "../utils/assert.h"
#include "../include/bits.h"
#include "physmem.h"
#include "virtmem.h"
#include "../utils/panic.h"
#include "../utils/mutex.h"
#include "../memory/kheap.h"
#include "../logger/logger.h"
#include "../arch/cpu.h"
#include "../klib/string.h"
#include "../memory/mem_region.h"

MODULE("VMM", LOG_LEVEL_TRACE);

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
    bool paging_enabled;
    page_dir_t page_directory; 
    phys_addr_t start_address;
    phys_addr_t end_address;
    phys_addr_t cutoff_address;
    mem_map_t *kernel_phys_map;

    // utility reserved addresses
    virt_addr_t workpg1_vaddr;
    virt_addr_t workpg2_vaddr;
    virt_addr_t workpg3_vaddr;

    // and their currently mapped physical addresses
    virt_addr_t workpg1_mapped_paddr;
    virt_addr_t workpg2_mapped_paddr;
    virt_addr_t workpg3_mapped_paddr;

    lock_t work_pages_lock;
} kernel_info;




static inline uint32_t make_pd_entry(uintptr_t address,
    bool cache_disable, bool write_through,
    bool user_access, bool write_enabled, bool page_present) {

    return
        (((uint32_t)address) & 0xFFFFF000) |
        (((uint32_t)cache_disable & 1) << 4) |
        (((uint32_t)write_through & 1) << 3) |
        (((uint32_t)user_access   & 1) << 2) |
        (((uint32_t)write_enabled & 1) << 1) |
        (((uint32_t)page_present  & 1));
}

static inline uint32_t make_pt_entry(uintptr_t address, bool global, bool page_attr_table,
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
static inline bool entry_is_present(uint32_t entry_value) {
    return (entry_value & 0x01);
}

// common to both page directory and page tables
static inline phys_addr_t _get_entry_address(uint32_t entry_value) {
    return (entry_value & 0xFFFFF000);
}

// common to both page directory and page tables
static inline uint32_t _get_table_entry(virt_addr_t table_address, uint32_t index) {
    return ((uint32_t *)table_address)[index];
}

// common to both page directory and page tables
static inline void _set_table_entry(virt_addr_t table_address, uint32_t index, uint32_t value) {
    ((uint32_t *)table_address)[index] = value;
}

// extracts the page directory entry num from a virtual address
static inline int page_dir_index(virt_addr_t address) {
    // highest 10 bits (31-22) are the entry of the page table
    return (int)((((uint32_t)address) >> 22) & 0x3FF);
}

// extracts the page table entry num from a virtual address
static inline int page_table_index(virt_addr_t address) { 
    // second 10 bits (21-12) are the entry of the page table
    return (int)((((uint32_t)address) >> 12) & 0x3FF);
}

// extracts the physical page offset from a virtual address
static inline uint32_t _virt_addr_to_physical_page_offset(virt_addr_t virtual_address) {
    // the lowest 12 (11-0) bits are offset into a 4KB space
    return ((uint32_t)virtual_address) & 0xFFF;
}

static void _map_page_primitive(virt_addr_t virtual_addr, phys_addr_t physical_addr);
static void _unmap_page_primitive(virt_addr_t virtual_addr);


// this work page is a reserve address that allows us to modify physical pages with temporary mapping
static inline virt_addr_t vmm_workpg1() { return kernel_info.workpg1_vaddr; }
static inline virt_addr_t vmm_workpg2() { return kernel_info.workpg2_vaddr; }
static inline void vmm_workpg1_map_to(phys_addr_t phys_addr) { _map_page_primitive(vmm_workpg1(), phys_addr); }
static inline void vmm_workpg2_map_to(phys_addr_t phys_addr) { _map_page_primitive(vmm_workpg2(), phys_addr); }
static inline void vmm_workpg1_unmap() { _unmap_page_primitive(vmm_workpg1()); }
static inline void vmm_workpg2_unmap() { _unmap_page_primitive(vmm_workpg2()); }

// --------------------------------------------------------

void vmm_initialize(phys_addr_t kernel_start_address, phys_addr_t kernel_end_address, phys_addr_t cutoff_address, phys_addr_t utility_pages_addr, size_t utility_pages_size, mem_map_t *kernel_phys_map) {
    log_trace("vmm_initialize(kstart=0x%x, kend=0x%x, cutoff=%d KB, uaddr=0x%x, usize=%d, kmap=0x%x)", kernel_start_address, kernel_end_address, cutoff_address / 1024, utility_pages_addr, utility_pages_size, kernel_phys_map);

    ASSERT(kernel_end_address > kernel_start_address);
    ASSERT(cutoff_address > kernel_end_address); // includes heap + stacks
    ASSERT(utility_pages_addr > 0);
    ASSERT(vmm_round_down(utility_pages_size) / vmm_page_size() >= 4);

    memset(&kernel_info, 0, sizeof(kernel_info));

    kernel_info.start_address = kernel_start_address;
    kernel_info.end_address = kernel_end_address;
    kernel_info.cutoff_address = cutoff_address;
    kernel_info.page_directory = utility_pages_addr + 0 * vmm_page_size();;
    kernel_info.workpg1_vaddr  = utility_pages_addr + 1 * vmm_page_size();
    kernel_info.workpg2_vaddr  = utility_pages_addr + 2 * vmm_page_size();
    kernel_info.workpg3_vaddr  = utility_pages_addr + 3 * vmm_page_size();
    kernel_info.kernel_phys_map = kernel_phys_map;

    // create a page directory for kernel.
    vmm_create_kernel_page_directory_using_physical_pages(kernel_info.page_directory, kernel_info.cutoff_address);
    
    // log_debug_hex((void *)kernel_info.page_directory, 16 * 4, 0);
    // vmm_dump_page_directory(kernel_info.page_directory);

    // now enable paging (fingers crossed!)
    vmm_set_page_directory_register(kernel_info.page_directory);
    vmm_enable_paging();
}

void vmm_create_kernel_page_directory_using_physical_pages(page_dir_t kernel_pd, virt_addr_t kernel_cutoff) {
    // Identity map the kernel before paging is enabled.
    ASSERT(kernel_pd != 0);
    ASSERT((kernel_cutoff & 0xFFF) == 0); // multiple of 4 KB

    memset((void *)kernel_pd, 0, vmm_page_size());

    for (virt_addr_t addr = 0; addr < kernel_cutoff; addr += 4096) {
        // Determine page directory index
        int pd_index = (int)(addr >> 22); // bits 31-22
        int pt_index = (int)((addr >> 12) & 0x3FF); // bits 21-12

        // Allocate page table if not already present
        uint32_t pd_entry = ((uint32_t *)kernel_pd)[pd_index];
        virt_addr_t page_table;
        if (entry_is_present(pd_entry)) {
            page_table = _get_entry_address(pd_entry);
        } else {
            page_table = pmm_allocate_physical_page();
            if (!page_table) panic("Cannot allocate kernel page table");
            memset((void *)page_table, 0, vmm_page_size());

            uint32_t entry = make_pd_entry(
                page_table,
                false, // cache disable
                false, // write through
                false, // user access
                true,  // write enable
                true   // present
            );
            ((uint32_t *)kernel_pd)[pd_index] = entry;
        }

        // Set the page table entry
        uint32_t pt_entry = make_pt_entry(
            addr,  // physical address
            true,  // global
            true,  // PAT
            false, // cache disable
            false, // write through
            false, // user access
            true,  // writable
            true   // present
        );
        ((uint32_t *)page_table)[pt_index] = pt_entry;
    }
}

virt_addr_t vmm_get_kernel_cutoff_address() {
    ASSERT(kernel_info.cutoff_address != 0);
    // this will be identity mapped anyway
    // used for task stacks, to allow kernel heap to grow
    return kernel_info.cutoff_address;
}


phys_addr_t vmm_resolve(virt_addr_t virtual_addr, page_dir_t page_dir_addr) {
    // For each virtual address, when we are dealing with 4K pages:
    //     10 bits 31-22 dictate the page directory entry (we find the table)
    //     10 bits 21-12 dictate the page table entry (we find the page)
    //     12 bits 11-0  dictate the byte within the page (12 bits address a 4KB space)
    int index;
    uint32_t entry;
    phys_addr_t address;

    // first resolve page directory.
    index = page_dir_index(virtual_addr);
    entry = _get_table_entry(page_dir_addr, index);
    if (!entry_is_present(entry))
        return 0;
    address = _get_entry_address(entry);
    if (address == 0)
        return 0;
    
    // then resolve page table
    index = page_table_index(virtual_addr);
    entry = _get_table_entry(address, index);
    if (!entry_is_present(entry))
        return 0;
    address = _get_entry_address(entry);
    if (address == 0)
        return 0;
    
    // now resolve final address
    uint32_t offset = _virt_addr_to_physical_page_offset(virtual_addr);
    return (address + offset);
}

void _map_page_primitive(virt_addr_t virtual_addr, phys_addr_t physical_addr) {
    // this function is expected operate on the work pages the kernel has.
    // it will not setup an new PTE, therefore it will not recurse.
    ASSERT(vmm_is_page_aligned(virtual_addr));
    ASSERT(vmm_is_page_aligned(physical_addr));

    page_dir_t pd_address = vmm_get_current_page_dir();
    ASSERT(pd_address != 0);

    // entry for page table MUST be there, we are in the first 4 MB
    int idx = page_dir_index(virtual_addr);
    uint32_t entry = ((uint32_t *)pd_address)[idx];
    ASSERT(entry_is_present(entry));

    // now, we may or may not have a value there.
    // no matter what was there, we will just rewrite it.
    phys_addr_t pt_address = (entry & 0xFFFFF000);
    ASSERT(pt_address != 0);
    idx = page_table_index(virtual_addr);
    entry = make_pt_entry(physical_addr, false, false, false, false, false, true, true);
    ((uint32_t *)pt_address)[idx] = entry;
    
    // invalidate for CPU to recalculate
    vmm_invalidate_cached_address(virtual_addr);
}

void _unmap_page_primitive(virt_addr_t virtual_addr) {
    // this function is expected operate on the work pages the kernel has.
    // it will not setup an new PTE, therefore it will not recurse.
    ASSERT(vmm_is_page_aligned(virtual_addr));
    page_dir_t pd_address = vmm_get_current_page_dir();
    ASSERT(pd_address != 0);

    // entry for page table MUST be there, we are in the first 4 MB
    int idx = page_dir_index(virtual_addr);
    uint32_t entry = ((uint32_t *)pd_address)[idx];
    ASSERT(entry_is_present(entry));

    // now, we may or may not have a value there.
    // no matter what was there, we will just rewrite it.
    phys_addr_t pt_address = (entry & 0xFFFFF000);
    ASSERT(pt_address != 0);
    idx = page_table_index(virtual_addr);
    ((uint32_t *)pt_address)[idx] = 0;
    
    // invalidate for CPU to recalculate
    vmm_invalidate_cached_address(virtual_addr);
}

error_t vmm_map_page_to_pd(virt_addr_t virtual_addr, virt_addr_t physical_addr, bool user_accessible, bool write_enable, page_dir_t page_dir) {
    log_trace("vmm_map_page_to_pd(virt=0x%x, phys=0x%x, page_dir=0x%x)", virtual_addr, physical_addr, page_dir);

    int index;
    uint32_t entry;

    // from the page directory, find or create the page table
    index = page_dir_index(virtual_addr);
    entry = ((uint32_t *)page_dir)[index];

    virt_addr_t page_table_paddr;
    if (entry_is_present(entry)) {
        page_table_paddr = _get_entry_address(entry);
    } else {
        page_table_paddr = pmm_allocate_physical_page();
        if (page_table_paddr == 0)
            return ERR_NO_MEMORY;
        
        // map/clear/unmap to initialize the PT
        _map_page_primitive(vmm_workpg1(), page_table_paddr);
        memset((void *)vmm_workpg1(), 0, vmm_page_size());
        _unmap_page_primitive(vmm_workpg1());

        
        // map/update/unmap, to add the new PT in the PD
        _map_page_primitive(vmm_workpg1(), page_dir);
        uint32_t page_dir_value = make_pd_entry(page_table_paddr, false, false, user_accessible, true, true);
        ((uint32_t *)vmm_workpg1())[index] = page_dir_value;
        _unmap_page_primitive(vmm_workpg1());
    }

    // map/update/unmap to set the entry in the PT
    _map_page_primitive(vmm_workpg1(), page_table_paddr);
    index = page_table_index(virtual_addr);
    entry = make_pt_entry(physical_addr, false, false, false, false, user_accessible, write_enable, true);
    ((uint32_t *)vmm_workpg1())[index] = entry;
    vmm_workpg1_unmap();
    
    return OK;
}

void vmm_unmap_page_from_pd(virt_addr_t virtual_addr, page_dir_t page_dir) {
    log_trace("vmm_unmap_page_from_pd(virt=0x%x, page_dir=0x%x)", virtual_addr, page_dir);

    // from the page directory, find or create the page table
    int index = page_dir_index(virtual_addr);
    uint32_t entry = ((uint32_t *)page_dir)[index];
    if (!entry_is_present(entry))
        return;

    // map/update/unmap to clear the entry in the PT
    phys_addr_t page_table_paddr = _get_entry_address(entry);
    _map_page_primitive(vmm_workpg1(), page_table_paddr);
    index = page_table_index(virtual_addr);
    ((uint32_t *)vmm_workpg1())[index] = 0;
    vmm_workpg1_unmap();
}


// map a range to itself
error_t vmm_identity_map_range(virt_addr_t start_addr, virt_addr_t end_addr, page_dir_t page_dir_addr) {
    log_trace("vmm_identity_map_range(start=0x%x, end=0x%x, page_dir=0x%x)", start_addr, end_addr, page_dir_addr);

    // we actually want to copy the contents of the kernel page directory into the new page directory.
    // i.e. the pointers to the page directories.
    // let's use the workpd and workpt to copy, 
    // since we don't know if the actual physical pages are accessible through current PD

    for (virt_addr_t addr = start_addr; addr < end_addr; addr += vmm_page_size()) {
        error_t err = vmm_map_page_to_pd(addr, addr, false, true, page_dir_addr);
        if (err) return err;
    }

    return OK;
}


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

inline void vmm_invalidate_cached_address(virt_addr_t virtual_addr) {
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

    kernel_info.paging_enabled = true;
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

    log_warn("Page fault, %s %s page by %s process, at address 0x%x, page dir is 0x%x (will map to continue)",
        write_attempt ? "writing on" : "reading a",
        page_present ? "protected" : "missing",
        supervisor_code ? "supervisor" : "user",
        memory_address,
        page_dir_address
    );

    // solution for now is to identity map this, just for fun
    // but, if we had a memory map (the mem_regions), we could identify who errored
    // e.g. stack underflow, or heap overflow, guard, mem-mapped file, etc
    
    error_t err = vmm_map_page_to_pd(vmm_round_down(memory_address), memory_address, true, true, page_dir_address);
}

// allocates and creates a new page directory
page_dir_t vmm_create_page_directory(bool map_kernel_space) {
    ASSERT(kernel_info.paging_enabled); // we rely on this

    page_dir_t page_dir = pmm_allocate_physical_page();
    if (page_dir == 0)
        return 0;

    log_trace("vmm_create_page_directory(), new PD 0x%x", page_dir);
    vmm_physpg_clear(page_dir);

    if (map_kernel_space) {
        log_debug("copying kernel PD contents to new page_directory");
        vmm_physpg_copy(page_dir, kernel_info.page_directory);
    }

    return page_dir;
}

// allocates pages and maps them to the virtual addresses requested (end address is non-inclusive)
error_t vmm_allocate_memory_range(virt_addr_t virt_addr_start, virt_addr_t virt_addr_end, page_dir_t page_dir_addr) {
    log_trace("vmm_allocate_memory_range(0x%p - 0x%p, PD=0x%p)", virt_addr_start, virt_addr_end, page_dir_addr);

    // TODO: this should update the memory map of the kernel/process
    for (virt_addr_t virt_addr = virt_addr_start; virt_addr < virt_addr_end; virt_addr += 4096) {
        virt_addr_t phys_page_addr = pmm_allocate_physical_page();
        error_t err = vmm_map_page_to_pd(virt_addr, phys_page_addr, true, true, page_dir_addr);
        // TODO: better error handling
    }

    return OK;
}

// frees any pointed pages, page tables, and the page directory itself
void vmm_destroy_page_directory(page_dir_t page_dir_address) {
    log_trace("vmm_destroy_page_directory(0x%x)", page_dir_address);
    pushcli();

    // free linked tables and pages 
    uint32_t entry;
    for (int pd_index = 0; pd_index < 1024; pd_index++) {
        entry = _get_table_entry(page_dir_address, pd_index);
        if (!entry_is_present(entry))
            continue;
        
        uintptr_t page_table_address = _get_entry_address(entry);
        if (page_table_address == 0)
            continue;

        // free any linked physical pages first
        for (int pt_index = 0; pt_index < 1024; pt_index++) {
            entry = _get_table_entry(page_table_address, pt_index);
            if (!entry_is_present(entry))
                continue;

            phys_addr_t phys_page_address = _get_entry_address(entry);
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
        log_debug("  Virt 0x%05xxxx             --> Phys 0x%05xxxx           %s", 
            virt_mem_group_start >> 12, 
            phys_mem_group_start >> 12,
            virt_mem_group_start == phys_mem_group_start ? "(identity)" : ""
        );
    } else {
        // group mapping
        log_debug("  Virt 0x%05xxxx..0x%05xxxx --> Phys 0x%05xxxx..0x%05xxxx  %d KB  %s", 
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

    // this version needs mapping to work first
    vmm_page_ops_t *ops = vmm_page_ops_for(page_dir_address);
    log_debug("Page directory at 0x%x mapping", page_dir_address);

    uint32_t entry;
    bool all_empty = true;

    // free linked tables and pages 
    _dump_page_directory_aggregate(1, 0, 0);
    for (int pd_index = 0; pd_index < 1024; pd_index++) {
        entry = ops->get_entry(page_dir_address, pd_index);
        if (!entry_is_present(entry))
            continue;
        all_empty = false;
        
        uintptr_t page_table_address = _get_entry_address(entry);
        if (page_table_address == 0)
            continue;

        // free any linked physical pages first
        for (int pt_index = 0; pt_index < 1024; pt_index++) {
            entry = ops->get_entry(page_table_address, pt_index);
            if (!entry_is_present(entry))
                continue;

            uint32_t physical_address = (uint32_t)_get_entry_address(entry);
            if (physical_address == 0)
                continue;
            
            uint32_t virtual_address = SET_BIT_RANGE(pd_index, 31, 22) | SET_BIT_RANGE(pt_index, 21, 12);
            _dump_page_directory_aggregate(2, virtual_address, physical_address);
        }
    }

    _dump_page_directory_aggregate(3, 0, 0);
    log_debug("Page directory at 0x%x end", page_dir_address);
}




// -------------------------------------------------------------------

/*
In order to operate on physical pages when they are not mapped
(e.g. when preparing a new page directory)
vmm needs to temporarily map them in known virtual address
Keeping track of what is mapped avoids double mapping time
Unmapping can be avoided as an optimization, used only when needed.
*/


static vmm_page_ops_t _mapped_page_ops = {
    .read      = vmm_physpg_read,
    .write     = vmm_physpg_write,
    .clear     = vmm_physpg_clear,
    .get_entry = vmm_physpg_get_entry,
    .set_entry = vmm_physpg_set_entry,
    .copy      = vmm_physpg_copy,
};
static vmm_page_ops_t _direct_page_ops = {
    .read      = vmm_direct_physpg_read,
    .write     = vmm_direct_physpg_write,
    .clear     = vmm_direct_physpg_clear,
    .get_entry = vmm_direct_physpg_get_entry,
    .set_entry = vmm_direct_physpg_set_entry,
    .copy      = vmm_direct_physpg_copy,
};

vmm_page_ops_t *vmm_page_ops_for(page_dir_t page_dir) {
    // TODO: convert elf loading to use this functionality
    page_dir_t curr = vmm_get_current_page_dir();
    return (!kernel_info.paging_enabled || page_dir == curr) ? &_direct_page_ops : &_mapped_page_ops;
}

// --------------------------------------------------------------

void vmm_physpg_read(virt_addr_t paddr, size_t offset, void *buffer, size_t size) {
    // mutex_acquire(&kernel_info.work_pages_lock);

    page_dir_t pd = vmm_get_current_page_dir();
    virt_addr_t pa = vmm_resolve(vmm_workpg1(), pd);

    _map_page_primitive(vmm_workpg1(), paddr);
    offset = min(offset, vmm_page_size());
    size = min(size, vmm_page_size() - offset);
    memcpy(buffer, (void *)vmm_workpg1() + offset, size);

    // mutex_release(&kernel_info.work_pages_lock);
}

void vmm_physpg_write(virt_addr_t paddr, size_t offset, void *buffer, size_t size) {
    // mutex_acquire(&kernel_info.work_pages_lock);

    _map_page_primitive(vmm_workpg1(), paddr);
    offset = min(offset, vmm_page_size());
    size = min(size, vmm_page_size() - offset);
    memcpy((void *)vmm_workpg1() + offset, buffer, size);

    // mutex_release(&kernel_info.work_pages_lock);
}

void vmm_physpg_clear(virt_addr_t paddr) {
    // mutex_acquire(&kernel_info.work_pages_lock);

    _map_page_primitive(vmm_workpg1(), paddr);
    memset((void *)vmm_workpg1(), 0, vmm_page_size());

    // mutex_release(&kernel_info.work_pages_lock);
}

uint32_t vmm_physpg_get_entry(virt_addr_t paddr, int index) {
    // mutex_acquire(&kernel_info.work_pages_lock);

    _map_page_primitive(vmm_workpg1(), paddr);
    index = clamp(index, 0, 1023);
    return ((uint32_t *)vmm_workpg1())[index];

    // mutex_release(&kernel_info.work_pages_lock);
}

void vmm_physpg_set_entry(virt_addr_t paddr, int index, uint32_t value) {
    // mutex_acquire(&kernel_info.work_pages_lock);

    _map_page_primitive(vmm_workpg1(), paddr);
    index = clamp(index, 0, 1023);
    ((uint32_t *)vmm_workpg1())[index] = value;

    // mutex_release(&kernel_info.work_pages_lock);
}

void vmm_physpg_copy(virt_addr_t pdest, virt_addr_t psource) {
    // mutex_acquire(&kernel_info.work_pages_lock);

    _map_page_primitive(vmm_workpg1(), pdest);
    _map_page_primitive(vmm_workpg2(), psource);
    memcpy((void *)vmm_workpg1(), (void *)vmm_workpg2(), vmm_page_size());

    // mutex_release(&kernel_info.work_pages_lock);
}


// --------------------------------------------

void vmm_direct_physpg_read(virt_addr_t paddr, size_t offset, void *buffer, size_t size) {
    offset = min(offset, vmm_page_size());
    size = min(size, vmm_page_size() - offset);
    memcpy(buffer, (void *)(paddr + offset), size);
}

void vmm_direct_physpg_write(virt_addr_t paddr, size_t offset, void *buffer, size_t size) {
    offset = min(offset, vmm_page_size());
    size = min(size, vmm_page_size() - offset);
    memcpy((void *)(paddr + offset), buffer, size);
}

void vmm_direct_physpg_clear(virt_addr_t paddr) {
    memset((void *)paddr, 0, vmm_page_size());
}

uint32_t vmm_direct_physpg_get_entry(virt_addr_t paddr, int index) {
    index = clamp(index, 0, 1023);
    return ((uint32_t *)paddr)[index];
}

void vmm_direct_physpg_set_entry(virt_addr_t paddr, int index, uint32_t value) {
    index = clamp(index, 0, 1023);
    ((uint32_t *)paddr)[index] = value;
}

void vmm_direct_physpg_copy(virt_addr_t pdest, virt_addr_t psource) {
    memcpy((void *)pdest, (void *)psource, vmm_page_size());
}

// ------------------------------------------
