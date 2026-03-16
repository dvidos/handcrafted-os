#include "../utils/assert.h"
#include "../include/bits.h"
#include "physmem.h"
#include "virtmem.h"
#include "../utils/panic.h"
#include "../logger/logger.h"
#include "../arch/cpu.h"
#include "../klib/string.h"
#include "../memory/mem_region.h"

MODULE("VMEM", LOG_LEVEL_TRACE);

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

    // utility reserved addresses
    virt_addr_t work_page1_addr;
    virt_addr_t work_page2_addr;
    virt_addr_t copy_page1_addr;
    virt_addr_t copy_page2_addr;
} kernel_info;


page_dir_t vmm_create_page_directory(bool map_kernel_space);


static inline uint32_t create_directory_entry_value(uintptr_t address,
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
static inline bool _is_entry_present(uint32_t entry_value) {
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
static inline int _virt_addr_to_page_directory_index(virt_addr_t virtual_address) {
    // highest 10 bits (31-22) are the entry of the page table
    return (int)((((uint32_t)virtual_address) >> 22) & 0x3FF);
}

// extracts the page table entry num from a virtual address
static inline int _virt_addr_to_page_table_index(virt_addr_t virtual_address) {
    // second 10 bits (21-12) are the entry of the page table
    return (int)((((uint32_t)virtual_address) >> 12) & 0x3FF);
}

// extracts the physical page offset from a virtual address
static inline uint32_t _virt_addr_to_physical_page_offset(virt_addr_t virtual_address) {
    // the lowest 12 (11-0) bits are offset into a 4KB space
    return ((uint32_t)virtual_address) & 0xFFF;
}

// this work page is a reserve address that allows us to modify physical pages with temporary mapping
static inline virt_addr_t vmm_workpd() { return kernel_info.work_page1_addr; }
static inline virt_addr_t vmm_workpt() { return kernel_info.work_page2_addr; }
static inline error_t vmm_workpd_map_to(phys_addr_t phys_addr) { error_t err; if ((err = vmm_map_page(vmm_workpd(), phys_addr, false, true)) != OK) return err; vmm_invalidate_cached_address(vmm_workpd()); return OK; }
static inline error_t vmm_workpt_map_to(phys_addr_t phys_addr) { error_t err; if ((err = vmm_map_page(vmm_workpt(), phys_addr, false, true)) != OK) return err; vmm_invalidate_cached_address(vmm_workpt()); return OK; }
static inline void vmm_workpd_unmap() { vmm_unmap_page(vmm_workpd()); vmm_invalidate_cached_address(vmm_workpd()); }
static inline void vmm_workpt_unmap() { vmm_unmap_page(vmm_workpt()); vmm_invalidate_cached_address(vmm_workpt()); }


// --------------------------------------------------------

void vmm_initialize(phys_addr_t kernel_start_address, phys_addr_t kernel_end_address, phys_addr_t utility_pages_addr, size_t utility_pages_size, mem_map_t *kernel_phys_map) {
    log_trace("vmm_initialize(kstart=0x%x, kend=0x%x, uaddr=0x%x, usize=%d, kmap=0x%x)", kernel_start_address, kernel_end_address, utility_pages_addr, utility_pages_size, kernel_phys_map);

    kernel_info.start_address = kernel_start_address;
    kernel_info.end_address = kernel_end_address;

    // reserved work addresses / pages
    ASSERT(utility_pages_addr > 0);
    ASSERT(vmm_round_down(utility_pages_size) / vmm_page_size() >= 4);
    kernel_info.work_page1_addr = utility_pages_addr + 0 * vmm_page_size();
    kernel_info.work_page2_addr = utility_pages_addr + 1 * vmm_page_size();
    kernel_info.copy_page1_addr = utility_pages_addr + 2 * vmm_page_size();
    kernel_info.copy_page2_addr = utility_pages_addr + 3 * vmm_page_size();

    // create a page directory for kernel.
    kernel_info.kernel_phys_map = kernel_phys_map;
    kernel_info.page_directory = vmm_create_kernel_page_directory_using_physical_pages(64 * MB);
    vmm_dump_page_directory(kernel_info.page_directory);

    
    // log_debug("Kernel page directory contents:");
    // log_debug_hex(kernel_page_direcory, 4096, (uint32_t)kernel_page_direcory);

    // void *pt = _get_entry_address(_get_table_entry(kernel_page_direcory, 0));
    // log_debug("Kernel first page table contents:");
    // log_debug_hex(pt, 4096, (uint32_t)pt);

    // void *va = (void *)(1024*1024 + 4096 + 7); // 1 MB
    // void *pa = vmm_resolve(va, kernel_page_direcory);
    // log_debug("Virtual address 0x%p resolves to physical address 0x%p", va, pa);

    // now enable paging (fingers crossed!)
    log_debug("Setting CR3 to kernel directory (0x%x)", kernel_info.page_directory);
    vmm_set_page_directory_register(kernel_info.page_directory);
    log_debug("Enabling paging");
    vmm_enable_paging();

    log_debug("Virtual memory paging initialized, range 0x%x - 0x%x will always be identity mapped");
}

phys_addr_t vmm_create_kernel_page_directory_using_physical_pages(phys_addr_t kernel_cutoff) {
    // Identity map the kernel before paging is enabled.
    ASSERT((kernel_cutoff & 0xFFF) == 0); // multiple of 4 KB

    phys_addr_t kernel_pd = pmm_allocate_physical_page();
    if (!kernel_pd) panic("Cannot allocate kernel PD");
    memset((void *)kernel_pd, 0, vmm_page_size());

    for (phys_addr_t addr = 0; addr < kernel_cutoff; addr += 4096) {
        // Determine page directory index
        int pd_index = (int)(addr >> 22); // bits 31-22
        int pt_index = (int)((addr >> 12) & 0x3FF); // bits 21-12

        // Allocate page table if not already present
        uint32_t pd_entry = ((uint32_t *)kernel_pd)[pd_index];
        phys_addr_t page_table;
        if (_is_entry_present(pd_entry)) {
            page_table = _get_entry_address(pd_entry);
        } else {
            page_table = pmm_allocate_physical_page();
            if (!page_table) panic("Cannot allocate kernel page table");
            memset((void *)page_table, 0, vmm_page_size());

            uint32_t entry = create_directory_entry_value(
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
        uint32_t pt_entry = create_table_entry_value(
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

    return kernel_pd;
}

virt_addr_t vmm_get_copy_page1_addr() {
    return kernel_info.copy_page1_addr;
}

virt_addr_t vmm_get_copy_page2_addr() {
    return kernel_info.copy_page2_addr;
}

virt_addr_t vmm_get_kernel_top_address() {
    ASSERT(kernel_info.end_address != 0);
    // this will be identity mapped anyway
    // used for task stacks, to allow kernel heap to grow
    return kernel_info.end_address;
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
    index = _virt_addr_to_page_directory_index(virtual_addr);
    entry = _get_table_entry(page_dir_addr, index);
    if (!_is_entry_present(entry))
        return 0;
    address = _get_entry_address(entry);
    if (address == 0)
        return 0;
    
    // then resolve page table
    index = _virt_addr_to_page_table_index(virtual_addr);
    entry = _get_table_entry(address, index);
    if (!_is_entry_present(entry))
        return 0;
    address = _get_entry_address(entry);
    if (address == 0)
        return 0;
    
    // now resolve final address
    uint32_t offset = _virt_addr_to_physical_page_offset(virtual_addr);
    return (address + offset);
}

error_t vmm_map_page(virt_addr_t virtual_addr, phys_addr_t physical_addr, bool user_accessible, bool write_enable) {
    page_dir_t page_dir = vmm_get_current_page_dir();
    log_trace("vmm_map_page(virt=0x%x, phys=0x%x), curr page_dir=0x%x)", virtual_addr, physical_addr, page_dir);

    int index;
    uint32_t entry;

    // from the page directory, find or create the page table
    index = _virt_addr_to_page_directory_index(virtual_addr);
    entry = _get_table_entry(page_dir, index);
    phys_addr_t page_table_paddr;
    if (_is_entry_present(entry)) {
        page_table_paddr = _get_entry_address(entry);
    } else {
        page_table_paddr = pmm_allocate_physical_page();
        if (page_table_paddr == 0)
            return ERR_NO_MEMORY;
        
        memset((void *)page_table_paddr, 0, vmm_page_size());

        uint32_t page_dir_value = create_directory_entry_value(
            page_table_paddr,
            true, // cache disable
            true, // write through
            user_accessible, // user accessible
            true, // write enabled
            true  // page present
        );

        // map, update, unmap
        _set_table_entry(page_dir, index, page_dir_value);
    }

    // map the table, read/write, unmap
    index = _virt_addr_to_page_table_index(virtual_addr);
    entry = _get_table_entry(page_table_paddr, index);
    if (!_is_entry_present(entry)) {
        entry = create_table_entry_value(
            physical_addr,
            true, // global
            true, // PAT
            true, // cache disable
            true, // write through
            user_accessible, // user accessible
            write_enable, // writable
            true  // page present
        );
        _set_table_entry(page_table_paddr, index, entry);
    }
    
    return OK;
}

error_t vmm_map_page_to_pd(virt_addr_t virtual_addr, phys_addr_t physical_addr, bool user_accessible, bool write_enable, page_dir_t page_dir) {
    log_trace("vmm_map_page_to_pd(virt=0x%x, phys=0x%x, page_dir=0x%x)", virtual_addr, physical_addr, page_dir);

    if (page_dir == vmm_get_current_page_dir())
        return vmm_map_page(virtual_addr, physical_addr, user_accessible, write_enable);

    int index;
    uint32_t entry;

    // from the page directory, find or create the page table
    index = _virt_addr_to_page_directory_index(virtual_addr);
    entry = _get_table_entry(page_dir, index);
    phys_addr_t page_table_paddr;
    if (_is_entry_present(entry)) {
        page_table_paddr = _get_entry_address(entry);
    } else {
        page_table_paddr = pmm_allocate_physical_page();
        if (page_table_paddr == 0)
            return ERR_NO_MEMORY;
        
        // map, clear, unmap
        vmm_workpt_map_to(page_table_paddr);
        memset((void *)vmm_workpt(), 0, vmm_page_size());
        vmm_workpt_unmap();

        uint32_t page_dir_value = create_directory_entry_value(
            page_table_paddr,
            true, // cache disable
            true, // write through
            user_accessible, // user accessible
            true, // write enabled
            true  // page present
        );

        // map, update, unmap
        vmm_workpd_map_to(page_dir);
        _set_table_entry(vmm_workpd(), index, page_dir_value);
        vmm_workpd_unmap();
    }

    // map the table, read/write, unmap
    vmm_workpt_map_to(page_table_paddr);
    index = _virt_addr_to_page_table_index(virtual_addr);
    entry = _get_table_entry(vmm_workpt(), index);
    if (!_is_entry_present(entry)) {
        entry = create_table_entry_value(
            physical_addr,
            true, // global
            true, // PAT
            true, // cache disable
            true, // write through
            user_accessible, // user accessible
            write_enable, // writable
            true  // page present
        );
        _set_table_entry(vmm_workpt(), index, entry);
    }
    vmm_workpt_unmap();
    
    return OK;
}

void vmm_unmap_page(virt_addr_t virtual_addr) {
    page_dir_t page_dir = vmm_get_current_page_dir();
    log_trace("vmm_unmap_page(vaddr=%x), curr page_dir=0x%x)", virtual_addr, page_dir);

    // find page table address
    int page_dir_index = _virt_addr_to_page_directory_index(virtual_addr);

    uint32_t page_dir_entry = _get_table_entry(page_dir, page_dir_index);

    if (!_is_entry_present(page_dir_entry))
        return; // no need, there's no page_table at all
    phys_addr_t page_table_address = _get_entry_address(page_dir_entry);
    
    // clear the page table entry
    uint32_t page_table_index = _virt_addr_to_page_table_index(virtual_addr);
    _set_table_entry(page_table_address, page_table_index, 0);
    bool pt_is_empty = memchk((void *)page_table_address, 0, vmm_page_size());

    if (pt_is_empty) {
        log_debug("Page table is clear, freeing physical page at 0x%x", page_table_address);
        pmm_free_physical_page(page_table_address);

        // remove entry from page directory
        _set_table_entry(page_dir, page_dir_index, 0);
    }
}

void vmm_unmap_page_from_pd(virt_addr_t virtual_addr, page_dir_t page_dir) {
    // clear the entry of the page table, if all the page table is clear, 
    // maybe remove the entry from the page directory and free the page.

    log_trace("vmm_unmap_page_from_pd(vaddr=%x, page_dir=0x%x)", virtual_addr, page_dir);

    if (page_dir == vmm_get_current_page_dir())
        return vmm_unmap_page(virtual_addr);

    // find page table address
    int page_dir_index = _virt_addr_to_page_directory_index(virtual_addr);

    vmm_workpd_map_to(page_dir);
    uint32_t page_dir_entry = _get_table_entry(vmm_workpd(), page_dir_index);
    vmm_workpd_unmap();

    if (!_is_entry_present(page_dir_entry))
        return; // no need, there's no page_table at all
    phys_addr_t page_table_address = _get_entry_address(page_dir_entry);
    
    // clear the page table entry
    uint32_t page_table_index = _virt_addr_to_page_table_index(virtual_addr);
    vmm_workpt_map_to(page_table_address);
    _set_table_entry(vmm_workpt(), page_table_index, 0);
    bool pt_is_empty = memchk((void *)vmm_workpt(), 0, vmm_page_size());
    vmm_workpt_unmap();

    if (pt_is_empty) {
        log_debug("Page table is clear, freeing physical page at 0x%x", page_table_address);
        pmm_free_physical_page(page_table_address);

        // remove entry from page directory
        vmm_workpd_map_to(page_dir);
        _set_table_entry(vmm_workpd(), page_dir_index, 0);
        vmm_workpd_unmap();
    }
}


// map a range to itself
error_t vmm_identity_map_range(phys_addr_t start_addr, phys_addr_t end_addr, page_dir_t page_dir_addr) {
    log_trace("vmm_identity_map_range(): copying kernel's PD (0x%x) to other PD (0x%x)", kernel_info.page_directory, page_dir_addr);

    // we actually want to copy the contents of the kernel page directory into the new page directory.
    // i.e. the pointers to the page directories.
    // let's use the workpd and workpt to copy, 
    // since we don't know if the actual physical pages are accessible through current PD

    vmm_workpd_map_to(kernel_info.page_directory);
    vmm_workpt_map_to(page_dir_addr);
    memcpy((void *)vmm_workpt(), (void *)vmm_workpd(), vmm_page_size());
    log_debug_hex((void *)vmm_workpt(), 16 * 16, 0);
    vmm_dump_page_directory(vmm_workpt());
    vmm_workpt_unmap();
    vmm_workpd_unmap();

    return OK;
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
    page_dir_t page_dir = pmm_allocate_physical_page();
    if (page_dir == INVALID_PAGE) {
        panic("Failed to allocate physical page for page directory!");
    }
    // we need to map this.
    log_debug("new page directory is 0x%x", page_dir);
    vmm_workpd_map_to(page_dir);
    memset((void *)vmm_workpd(), 0x00, PAGE_SIZE);
    vmm_workpd_unmap();

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
error_t vmm_allocate_memory_range(virt_addr_t virt_addr_start, virt_addr_t virt_addr_end, page_dir_t page_dir_addr) {
    log_trace("vmm_allocate_memory_range(0x%p - 0x%p, PD=0x%p)", virt_addr_start, virt_addr_end, page_dir_addr);

    // TODO: this should update the memory map of the kernel/process
    for (virt_addr_t virt_addr = virt_addr_start; virt_addr < virt_addr_end; virt_addr += 4096) {
        phys_addr_t phys_page_addr = pmm_allocate_physical_page();
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
        if (!_is_entry_present(entry))
            continue;
        
        uintptr_t page_table_address = _get_entry_address(entry);
        if (page_table_address == 0)
            continue;

        // free any linked physical pages first
        for (int pt_index = 0; pt_index < 1024; pt_index++) {
            entry = _get_table_entry(page_table_address, pt_index);
            if (!_is_entry_present(entry))
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
    bool all_empty = true;

    // free linked tables and pages 
    _dump_page_directory_aggregate(1, 0, 0);
    for (int pd_index = 0; pd_index < 1024; pd_index++) {
        entry = _get_table_entry(page_dir_address, pd_index);
        if (!_is_entry_present(entry))
            continue;
        all_empty = false;
        
        uintptr_t page_table_address = _get_entry_address(entry);
        if (page_table_address == 0)
            continue;

        // free any linked physical pages first
        for (int pt_index = 0; pt_index < 1024; pt_index++) {
            entry = _get_table_entry(page_table_address, pt_index);
            if (!_is_entry_present(entry))
                continue;

            uint32_t physical_address = (uint32_t)_get_entry_address(entry);
            if (physical_address == 0)
                continue;
            
            uint32_t virtual_address = SET_BIT_RANGE(pd_index, 31, 22) | SET_BIT_RANGE(pt_index, 21, 12);
            _dump_page_directory_aggregate(2, virtual_address, physical_address);
        }
    }
    _dump_page_directory_aggregate(3, 0, 0);
}
