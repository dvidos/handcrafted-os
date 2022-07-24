#include <bits.h>
#include <memory/physmem.h>
#include <drivers/screen.h>
#include <klog.h>
#include <klib/string.h>
#include <cpu.h>


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

void *create_page_directory(bool map_kernel_space);



static inline uint32_t create_directory_entry_value(void *address, bool cache_disable, bool write_through,
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

static inline uint32_t create_table_entry_value(void *address, bool global, bool page_attr_table,
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
static inline bool is_entry_present(uint32_t entry_value) {
    return (entry_value & 0x01);
}

// common to both page directory and page tables
static inline void *get_entry_address(uint32_t entry_value) {
    return (void *)(entry_value & 0xFFFFF000);
}

// common to both page directory and page tables
static inline uint32_t get_table_entry(void *table_address, uint32_t index) {
    return ((uint32_t *)table_address)[index];
}

// common to both page directory and page tables
static inline void set_table_entry(void *table_address, uint32_t index, uint32_t value) {
    ((uint32_t *)table_address)[index] = value;
}

// extracts the page directory entry num from a virtual address
static inline uint32_t virt_addr_to_page_directory_index(void *virtual_address) {
    // highest 10 bits (31-22) are the entry of the page table
    return (((uint32_t)virtual_address) >> 22) & 0x3FF;
}

// extracts the page table entry num from a virtual address
static inline uint32_t virt_addr_to_page_table_index(void *virtual_address) {
    // second 10 bits (21-12) are the entry of the page table
    return (((uint32_t)virtual_address) >> 12) & 0x3FF;
}

// extracts the physical page offset from a virtual address
static inline uint32_t virt_addr_to_physical_page_offset(void *virtual_address) {
    // the lowest 12 (11-0) bits are offset into a 4KB space
    return ((uint32_t)virtual_address) & 0xFFF;
}


void *resolve_virtual_to_physical_address(void *virtual_addr, void *page_dir_addr) {
    // For each virtual address, when we are dealing with 4K pages:
    //     10 bits 31-22 dictate the page directory entry (we find the table)
    //     10 bits 21-12 dictate the page table entry (we find the page)
    //     12 bits 11-0  dictate the byte within the page (12 bits address a 4KB space)
    uint32_t index, entry;
    void *address;

    // first resolve page directory.
    index = virt_addr_to_page_directory_index(virtual_addr);
    entry = get_table_entry(page_dir_addr, index);
    if (!is_entry_present(entry))
        return NULL;
    address = get_entry_address(entry);
    if (address == NULL)
        return NULL;
    
    // then resolve page table
    index = virt_addr_to_page_table_index(virtual_addr);
    entry = get_table_entry(address, index);
    if (!is_entry_present(entry))
        return NULL;
    address = get_entry_address(entry);
    if (address == NULL)
        return NULL;
    
    // now resolve final address
    uint32_t offset = virt_addr_to_physical_page_offset(virtual_addr);
    return (void *)(address + offset);
}

// map the virtual address to resolve to the physical one for the particular page directory.
void map_virtual_address_to_physical(void *virtual_addr, void *physical_addr, void *page_dir_addr, bool skip_logging) {
    if (!skip_logging) {
        klog_trace("Mapping phys addr 0x%x to virt addr 0x%x, page dir 0x%x", physical_addr, virtual_addr, page_dir_addr);
    }

    uint32_t page_dir_index = virt_addr_to_page_directory_index(virtual_addr);
    uint32_t page_dir_entry = get_table_entry(page_dir_addr, page_dir_index);
    void *page_table_address;
    // klog_debug("pd address = 0x%p, pd index = %d, pd entry = 0x%x", page_dir_addr, page_dir_index, page_dir_entry);

    if (is_entry_present(page_dir_entry)) {
        page_table_address = get_entry_address(page_dir_entry);
    } else {
        // we need to create one
        page_table_address = allocate_physical_page((void *)0);
        klog_debug("Allocated new physical page at 0x%p for new page table", page_table_address);
        memset(page_table_address, 0, 4096);
        uint32_t page_dir_value = create_directory_entry_value(
            page_table_address,
            true, // cache disable
            true, // write through
            true, // user accessible
            true, // write enabled
            true  // page present
        );
        // klog_debug("new page_dir entry value = 0x%08x", page_dir_value);
        set_table_entry(page_dir_addr, page_dir_index, page_dir_value);
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
        true, // user accessible
        true, // writable
        true  // page present
    );
    // klog_debug("new page_table entry value = 0x%08x", page_table_entry);
    set_table_entry(page_table_address, page_table_index, page_table_entry);
}

// unmap the virtual address to resolve to the physical one for the particular page directory.
void unmap_virtual_address(void *virtual_addr, void *page_dir_addr) {
    // clear the entry of the page table, if all the page table is clear, 
    // maybe remove the entry from the page directory and free the page.

    klog_trace("Unmapping virt addr 0x%x, page dir 0x%x", virtual_addr, page_dir_addr);

    // find page table address
    uint32_t page_dir_index = virt_addr_to_page_directory_index(virtual_addr);
    uint32_t page_dir_entry = get_table_entry(page_dir_addr, page_dir_index);
    if (!is_entry_present(page_dir_entry))
        return; // no need, there's no page_table at all
    void *page_table_address = get_entry_address(page_dir_entry);
    
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
        klog_debug("Page table is clear, freeing physical page at 0x%x", page_table_address);
        free_physical_page(page_table_address);
        // remove entry from page directory
        set_table_entry(page_dir_addr, page_dir_index, 0);
    }
}

// map a range to itself
void identity_map_range(void *start_addr, void *end_addr, void *page_dir_addr) {
    klog_trace("Identity mapping range 0x%p - 0x%p, page_dir=0x%p", start_addr, end_addr, page_dir_addr);
    for (void *addr = start_addr; addr <= end_addr; addr += 4096) {
        map_virtual_address_to_physical(addr, addr, page_dir_addr, true);
    }
}


// Enabling paging is actually very simple. All that is needed is 
// to load CR3 with the address of the page directory 
// and to set the paging (PG) and protection (PE) bits of CR0.
void set_page_directory_register(void *address) {
    klog_trace("Setting CR3 to 0x%x", address);

    __asm__ __volatile__(
        "mov %0, %%eax\n\t"
        "mov %%eax, %%cr3\n\t"
        :             // no output values
        : "g"(address)  // input No 0
        : "eax"       // garbled registers
    );
}

// get current directory register
void *get_page_directory_register() {
    uint32_t value;

    __asm__ __volatile__(
        "movl %%cr3, %0\n\t"
        : "=g"(value)  // output No 0
        :              // no input values
        :              // garbled registers
    );

    return (void *)value;
}

inline void invalidate_paging_cached_address(void *virtual_addr) {
    // supported on i486+
    asm volatile("invlpg (%0)" : : "r" ((uint32_t)virtual_addr) : "memory");
}

static void enable_memory_paging_cpu_bit() {
    klog_trace("Enabling memory paging in CPU");

    __asm__ __volatile__(
        "mov %%cr0, %%eax\n\t"
        "or $0x80000000, %%eax\n\t"  // turn on bit 31
        "mov %%eax, %%cr0"
        :       // no outputs
        :       // no inputs
        : "eax" // garbled registers
    );
}

static void disable_memory_paging_cpu_bit() {
    klog_trace("Disabling memory paging in CPU");

    __asm__ __volatile__(
        "mov %%cr0, %%eax\n\t"
        "and $0x7FFFFFFF, %%eax\n\t"  // turn off bit 31
        "mov %%eax, %%cr0"
        :       // no outputs
        :       // no inputs
        : "eax" // garbled registers
    );
}

static struct {
    void *page_directory; 
    void *start_address;
    void *end_address;
} kernel_info;

void init_virtual_memory_paging(void *kernel_start_address, void *kernel_end_address) {
    if (physical_page_size() != 4096)
        panic("Virtual memory supports 4KB pages only");
    
    kernel_info.start_address = kernel_start_address;
    kernel_info.end_address = kernel_end_address;

    // create a page directory for kernel.
    kernel_info.page_directory = create_page_directory(true);

    // klog_debug("Kernel page directory contents:");
    // klog_hex16_debug(kernel_page_direcory, 4096, (uint32_t)kernel_page_direcory);

    // void *pt = get_entry_address(get_table_entry(kernel_page_direcory, 0));
    // klog_debug("Kernel first page table contents:");
    // klog_hex16_debug(pt, 4096, (uint32_t)pt);

    // void *va = (void *)(1024*1024 + 4096 + 7); // 1 MB
    // void *pa = resolve_virtual_to_physical_address(va, kernel_page_direcory);
    // klog_debug("Virtual address 0x%p resolves to physical address 0x%p", va, pa);

    // now enable paging (fingers crossed!)
    set_page_directory_register(kernel_info.page_directory);
    enable_memory_paging_cpu_bit();
}

void *get_kernel_page_directory() {
    return kernel_info.page_directory;
}

// handles page faults. 
// see https://wiki.osdev.org/Exceptions#Page_Fault
void virtual_memory_page_fault_handler(uint32_t error_code) {
    // CR2 contains the virtual address that caused the error.
    bool page_present    = IS_BIT(error_code, 0);
    bool write_attempt   = IS_BIT(error_code, 1);
    bool supervisor_code = IS_BIT(error_code, 2);

    uint32_t memory_address = 0;
    __asm__ __volatile__("mov %%cr2, %0" : "=g"(memory_address));

    void *page_dir_address = 0;
    __asm__ __volatile__("mov %%cr3, %0" : "=g"(page_dir_address));

    klog_warn("Page fault, %s %s page by %s process, at 0x%x, page dir at 0x%p",
        write_attempt ? "writing on" : "reading a",
        page_present ? "protected" : "missing",
        supervisor_code ? "supervisor" : "user",
        memory_address,
        page_dir_address
    );

    // solution for now is to identity map this, just for fun
    memory_address = ROUND_DOWN_4K(memory_address);
    map_virtual_address_to_physical((void *)memory_address, (void *)memory_address, page_dir_address, false);
}




// allocates and creates a new page directory
void *create_page_directory(bool map_kernel_space) {
    void *page_dir = allocate_physical_page((void *)0);
    memset(page_dir, 0, physical_page_size());

    if (map_kernel_space) {
        // the kernel (code, data, heap etc) must be mapped in the same address in all address spaces.
        // that way, we can switch CR3 and jump into an elf loading function without issues.
        // or execute kerel code, or keep variables and pointers sane when switching tasks
        identity_map_range(kernel_info.start_address, kernel_info.end_address, page_dir);
    }

    klog_trace("create_page_directory() -> 0x%p", page_dir);
    return page_dir;
}

// allocates pages and maps them to the virtual addresses requested
// end address is non-inclusive
void allocate_virtual_memory_range(void *virt_addr_start, void *virt_addr_end, void *page_dir_addr) {
    klog_trace("allocate_virtual_memory_range(0x%p - 0x%p, PD=0x%p)", virt_addr_start, virt_addr_end, page_dir_addr);

    for (void *virt_addr = virt_addr_start; virt_addr < virt_addr_end; virt_addr += 4096) {
        void *phys_page_addr = allocate_physical_page((void *)0x100000);
        map_virtual_address_to_physical(virt_addr, phys_page_addr, page_dir_addr, false);
    }
}

// frees any pointed pages, page tables, and the page directory itself
void destroy_page_directory(void *page_dir_address) {
    klog_trace("destroy_page_directory(0x%x)", page_dir_address);
    pushcli();

    // free linked tables and pages 
    uint32_t entry;
    for (int pd_index = 0; pd_index < 1024; pd_index++) {
        entry = get_table_entry(page_dir_address, pd_index);
        if (!is_entry_present(entry))
            continue;
        
        void *page_table_address = get_entry_address(entry);
        if (page_table_address == NULL)
            continue;

        // free any linked physical pages first
        for (int pt_index = 0; pt_index < 1024; pt_index++) {
            entry = get_table_entry(page_table_address, pt_index);
            if (!is_entry_present(entry))
                continue;

            void *phys_page_address = get_entry_address(entry);
            if (phys_page_address == 0)
                continue;

            free_physical_page(phys_page_address);
        }

        free_physical_page(page_table_address);
    }

    // we can now free the page directory itself
    free_physical_page(page_dir_address);
    popcli();
}

