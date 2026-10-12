# Interfaces

I think, in order to decouple the following big pieces of the kernel,
and be able to test them in isolation, we'll need to establish
some interfaces, i.e. structs with pointer to functions that represent
a dependency.

* Physical Memory Manager (no dependencies)
* Virtual Memory Manager, depends on
  * Physical memory manager
  * A process interface for terminating a process that made invalid memory poperation
* Kernel Heap (may depend on physical memory manager)
* Virtual File System (depends on kernel heap)
* Process Manager, depends on:
  * Virtual Memory Manager, in order to create memory spaces for processes
  * Kernel Heap, for allocating memory
  * Some file system node interface, in order to map memory to a file

## Example code

```c

// ------------------------------------------------------

// formatting as a structure allows for finding out who has implementations of the interface
typedef struct panic_button {
    void (*panic)(const char *fmt, ...);
} panic_button;


// ------------------------------------------------------



// ------------------------------------------------------

// logger, appender

// ------------------------------------------------------

typedef struct debug_stream debug_stream;
struct debug_stream {
    void (*printf)(debug_stream *s, const char *fmt, ...);
};

typedef struct debuggable debuggable;
typedef enum debug_options debug_options;

// most major structs should be debuggable, to aid in inspection
struct debuggable {
    void (*debug_on_stream)(void *instance, debug_stream *stream, int options);
};

enum debug_options {
    DEBUG_OPTIONS_DETAILED      = 1,
    DEBUG_OPTIONS_CONCISE       = 2,
    DEBUG_OPTIONS_SINGLE_LINE   = 4,
    DEBUG_OPTIONS_MULTI_LINE    = 8,
};  

// ------------------------------------------------------

// initialized with memory map, predictions, tuning, etc.
// some pages reserved (kernel code & data)
// some pages for the bitmap, some others for counting of shared pages
typedef struct phys_page_provider phys_page_provider;
struct phys_page_provider {
    size_t (*total_pages)(phys_page_provider *p);
    size_t (*available_pages)(phys_page_provider *p);

    // allocates one or more pages. usage counter set to 1
    phys_addr_t (*allocate_pages)(phys_page_provider *p, size_t num_pages);

    // releases one or more pages. their counter must be one
    error_t (*release_pages)(phys_page_provider *p, phys_addr_t addr, size_t num_pages);

    // increase allocated page counter. assumed allocated
    error_t (*retain_pages)(phys_page_provider *p, phys_addr_t addr, size_t num_pages);

    // increase / decrease counter. if zero, release
    error_t (*release_pages)(phys_page_provider *p, phys_addr_t addr, size_t num_pages);
};

// ------------------------------------------------------

// internally there must be the mapping-operating-unmapping process, as well as the Mapping Window
// a vm_area belongs to one page directory and may contain one or more of vm_mappings.
// a vm_mapping is a virt address mapped to one or more physical pages

struct vm_area {
    // ??? size? flags? is ok?
    // internal addresses, size, flags, possibly backing node, etc
    // possibly (static) name?
    // has link to next area
    
    debuggable (*get_debug_funcs)(vm_area *area);
};

// provides all the VMM needs to map a file to a memory area
struct vfs_memmappable_node {
    error_t (*read)(vfs_memmappable_node *node, void *target_addr, size_t offset, size_t bytes);
    error_t (*write)(vfs_memmappable_node *node, void *target_addr, size_t offset, size_t bytes);

    debuggable (*get_debug_funcs)(vfs_memmappable_node *node);
};


typedef enum vm_area_flags vm_area_flags;
typedef struct vm_space vm_space;
struct vm_space {
    // has one page directory address, owns zero to many areas
    phys_addr_t (*get_page_directory_addr)(vm_space *space);

    // map to some arbitrary physical memory
    error_t (*map_area_to_memory)(vm_space *space, virt_addr_t virt_addr, size_t size_bytes, vm_area_flags flags);

    // map to specific physical memory (framebuffer, mapped I/O, etc)
    error_t (*map_area_to_phys_address)(vm_space *space, virt_addr_t virt_addr, phys_addr_t phys_address, size_t size_bytes, vm_area_flags flags);

    // maps an area to a file, for reading or writing
    error_t (*map_area_to_file)(vm_space *space, virt_addr_t virt_addr, vfs_memmappable_node *node, size_t file_offset, vm_area_flags flags);

    // unmaps an area and releases anything related to it.
    error_t (*unmap_area)(vm_space *space, virt_addr_t virt_addr);

    debuggable (*get_debug_funcs)(vm_space *space);
};

enum vm_area_flags {
    VM_READ    = (1UL << 0),
    VM_WRITE   = (1UL << 1),    // allow writing
    VM_EXEC    = (1UL << 2),
    VM_USER    = (1UL << 3),    // allow access from ring 3
    VM_SHARED  = (1UL << 4),    // shared between processes etc.
    VM_STACK   = (1UL << 5),
    VM_NOCACHE = (1UL << 6),    // disable caching (PCD, PWT) for direct write to mapped memory (e.g. framebuffer)
    VM_ANON    = (1UL << 7),    // anonymous memory, release at destruction
    VM_DIRECT  = (1UL << 8),    // MMIO/framebuffer, skip release at destruction (not necessarily identity mapping)
    VM_AUTO_EXPAND_BEFORE = (1UL <<  9),  // guard page for underflow (i.e. stack)
    VM_AUTO_EXPAND_AFTER  = (1UL << 10),  // guard page for overflow (i.e. heap)
};

struct virtual_memory_provider_for_process_manager {
    // create a space for a process
    error_t (*create_space)(virtual_memory_provider_for_process_manager *provider, vm_space **space_ptr);

    // release all areas and memories
    error_t (*destroy_space)(virtual_memory_provider_for_process_manager *provider, vm_space *space);
};

// optimization: we need  to think if how the interface + instance data fit together
// either a hidden pointer or something...
// then, we need to fix aider coding with ollama

// process also needs a vfs operations, for opening files (stdin, out, err, etc) etc.
// process loader needs a basic vfs operations, to load executables

// ------------------------------------------------------

struct block_device {
    size_t (*block_size)(block_device *d);
    size_t (*num_blocks)(block_device *d);

    error_t (*read_block)(block_device *d, size_t block_no, void *buffer);
    error_t (*write_block)(block_device *d, size_t block_no, void *buffer);

    debuggable (*get_debug_funcs)(block_device *d);
};

struct char_device {
    error_t (*read)(char_device *d, void *buffer, size_t size);
    error_t (*write)(char_device *d, void *buffer, size_t size);

    debuggable (*get_debug_funcs)(char_device *d);
};


```
