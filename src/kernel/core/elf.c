#include <stddef.h>
#include <errors.h>
#include <klog.h>
#include <filesys/vfs.h>
#include <memory/physmem.h>
#include <memory/kheap.h>
#include <klib/string.h>
#include <multitask/process.h>
#include <devices/tty.h>

#define min(a, b)   ((a) <= (b) ? (a) : (b))
#define max(a, b)   ((a) >= (b) ? (a) : (b))

// see http://www.skyfree.org/linux/references/ELF_Format.pdf

typedef uint32_t elf32_addr;
typedef uint16_t elf32_half;
typedef uint32_t elf32_offs;
typedef int32_t  elf32_sword;
typedef uint32_t elf32_word;
typedef uint8_t  elf32_char;

// elf header at the start of a file
typedef struct {
    uint8_t  identification[16] ;
    elf32_half type;       // see ELF_TYPE_x defines
    elf32_half machine;    // see ELF_MACHINE defines
    elf32_word version;    // see ELF_VERSION 
    elf32_addr entry;      // entry point address
    elf32_offs phoff;      // start of program headers (bytes into the file)
    elf32_offs shoff;      // start of section headers (bytes into the file)
    elf32_word flags;      // see ELF_MACHINE_FLAGS_ defines
    elf32_half ehsize;     // elf header size. in bytes
    elf32_half phentsize;  // size of each program header table entry, in bytes
    elf32_half phnum;      // number of entries in program header table
    elf32_half shentsize;  // size of each section header table entry, in bytes
    elf32_half shnum;      // number of entries in section header table
    elf32_half shstrndx;   // section name string table index
} elf32_header_t;

// elf file types
#define ELF_TYPE_NONE          0
#define ELF_TYPE_RELOCATABLE   1
#define ELF_TYPE_EXECUTABLE    2
#define ELF_TYPE_DYNAMIC       3 // shared object file
#define ELF_TYPE_CORE          4
#define ELF_TYPE_LO_PROC       0xFF00  // processor specific start range
#define ELF_TYPE_HI_PROC       0xFFFF  // processor specific end range (inclusive)

// machine types (others omitted)
#define ELF_MACHINE_80386      3

// versions (others omitted)
#define ELF_VERSION_CURRENT    1

/**
 * Section Headers:
 * - everything in an elf file (except the header, the program header table 
 *       and the section header table) are contained in a section
 * - each section has it header
 * - sections can be empty, but they are not overlapping, and there may be gaps
 */
typedef struct {
    elf32_word sh_name;      // name of the section. it's an index into the section header string table
    elf32_word sh_type;      // type of section
    elf32_word sh_flags;     // section flags
    elf32_addr sh_addr;      // if non-zero, it's the address the first byte should reside in an image
    elf32_offs sh_offset;    // offset in the file (in bytes) of the section content
    elf32_word sh_size;      // section size in bytes
    elf32_word sh_link;      // section header table index link, depends on type
    elf32_word sh_info;      // depends on type
    elf32_word sh_addralign; // alignment needs for content. powers of two only.
    elf32_word sh_entsize;   // for entries of fixed size, this contains the entry size
} elf32_section_header_t;

// section types
#define ELF_SHT_NULL       0  // inactive section
#define ELF_SHT_PROGBITS   1  
#define ELF_SHT_SYMTAB     2  // symbol table
#define ELF_SHT_STRTAB     3  // string table
#define ELF_SHT_RELA       4  // relocation entries, with explicit addends
#define ELF_SHT_HASH       5  // dynamic linking hash table
#define ELF_SHT_DYNAMIC    6  // dynamic linking information
#define ELF_SHT_NOTE       7  // 
#define ELF_SHT_NOBITS     8  // empty section
#define ELF_SHT_REL        9  // relocation entries, without explicit addends
#define ELF_SHT_SHLIB     10  // 
#define ELF_SHT_DYNSYM    11  // symbol table

// section flags
#define ELF_SHF_WRITE       0x1  // contains data that should be writable during execution
#define ELF_SHF_ALLOC       0x2  // occupies memory during execution
#define ELF_SHF_EXEC_INSTR  0x4  // contains executable machine instructions


// program segment header
typedef struct {
    elf32_word p_type;    // segment type, see PT_x defines
    elf32_offs p_offset;  // offset in bytes in file of the first byte of the segment
    elf32_addr p_vaddr;   // address of first byte in memory
    elf32_addr p_paddr;   // for physical addressing, where required. ignore.
    elf32_word p_filesz;  // number of bytes in the file   image of segment. may be zero
    elf32_word p_memsz;   // number of bytes in the memory image of segment. may be zero
    elf32_word p_flags;   // segment flags, see below
    elf32_word p_align;   // required alignment no for segments (power of 2)
} elf32_program_header_t;

// program segment type defines
#define PT_NULL     0  // unused entry
#define PT_LOAD     1  // loadable segment. if file size < memory size, fill with zeros.
#define PT_DYNAMIC  2  // dynamic linking information
#define PT_INTERP   3  // location and size of program to use as interpreter
#define PT_NOTE     4  // auxiliary information
#define PT_SHLIB    5  // reserved, unspecified, ignore.
#define PT_PHDR     6  // specifies location and size of the program header table

// program segment flags
#define PF_EXEC    0x1  // executable
#define PF_WRITE   0x2  // writable
#define PF_READ    0x4  // readable


typedef struct {
    elf32_word st_name;  // name 
    elf32_addr st_value; // value, address, etc
    elf32_word st_size;  // size (e.g. for variables)
    uint8_t    st_info;  // high nibble: bind type (see STB_x), low nibble: type (see STT_x)
    uint8_t    st_other; // ignore 
    elf32_half st_shndx;
} elf32_symbol_t;

// symbol bind types
#define STB_LOCAL    0
#define STB_GLOBAL   1
#define STB_WEAK     2

// symbol type types
#define STT_NOTYPE   0
#define STT_OBJECT   1  // variable, array etc
#define STT_FUNC     2  // function
#define STT_SECTION  3
#define STT_FILE     4




// examines the first 16 bytes of an ELF file
static int check_elf_identification(char *identification) {

    if (identification[0] != 0x7F)
        return ERR_NOT_SUPPORTED;

    if (memcmp(identification + 1, "ELF", 3) != 0)
        return ERR_NOT_SUPPORTED;

    // offset 4 is file class (1=32 bits, 2=64 bits)
    if (identification[4] != 1)
        return ERR_NOT_SUPPORTED;
    
    // offset 5 is data encoding (1=lsb, 2=msb)
    // 1 = LSB in lowest address, 2's complement data, little endian, what I have in my laptop
    //     e.g. 0x01020304 is stored in memory as bytes [ 0x04 ][ 0x03 ][ 0x02 ][ 0x01 ]
    // 2 = MSB in lowest address, 2's complement data, big endian
    //     e.g. 0x01020304 is stored in memory as bytes [ 0x01 ][ 0x02 ][ 0x03 ][ 0x04 ]
    if (identification[5] != 1)
        return ERR_NOT_SUPPORTED;

    // offset 6 is version. It has to be the "current" version
    // current version we support is 1.
    if (identification[6] != 1)
        return ERR_NOT_SUPPORTED;
 
    return SUCCESS;
}


static void debug_elf_header(elf32_header_t *header) {
    char *elf_types[] = {
        "NONE",
        "RELOCATABLE",
        "EXECUTABLE",
        "DYNAMIC",
        "CORE"
    };
    klog_debug("ELF identification: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
        header->identification[0],
        header->identification[1],
        header->identification[2],
        header->identification[3],
        header->identification[4],
        header->identification[5],
        header->identification[6],
        header->identification[7],
        header->identification[8],
        header->identification[9],
        header->identification[10],
        header->identification[11],
        header->identification[12],
        header->identification[13],
        header->identification[14],
        header->identification[15]
    );

    klog_debug("  elf type:                   %d (%s)", header->type, elf_types[header->type]);
    klog_debug("  machine:                    %d (3 = 386)", header->machine);
    klog_debug("  version:                    %d", header->version);
    klog_debug("  entry point address:        0x%x", header->entry);
    klog_debug("  start of program headers:   0x%x", header->phoff);
    klog_debug("  start of section headers:   0x%x", header->shoff);
    klog_debug("  flags:                      0x%x", header->flags);
    klog_debug("  elf header size:            0x%x", header->ehsize);
    klog_debug("  program header entry size:  0x%x", header->phentsize);
    klog_debug("  program headers count:      %d", header->phnum);
    klog_debug("  section header entry size:  0x%x", header->shentsize);
    klog_debug("  section headers count:      %d", header->shnum);
    klog_debug("  section name string index:  %d", header->shstrndx);
}

static void debug_elf_section_header(bool title_line, int num, elf32_section_header_t *section, char *names_data) {
    char *stypes[] = {
        "NULL",
        "PROGBITS",
        "SYMTAB",
        "STRTAB",
        "RELA",
        "HASH",
        "DYNAMIC",
        "NOTE",
        "NOBITS",
        "REL",
        "SHLIB",
        "DYNSYM"
    };

    if (title_line) {
        klog_debug("  No Name             Type         Addr     Offset   Size     ES Flg Lk Inf Al");
        //            XX 1234567890123456 123456789012 12345678 12345678 12345678
    } else {
        klog_debug("  %2d %-16s %-12s %08x %08x %08x %02x %c%c%c %2d %3d %2d",
            num,
            names_data == NULL ? "?" : names_data + section->sh_name, // we have to resolve this.
            stypes[section->sh_type],
            section->sh_addr,
            section->sh_offset,
            section->sh_size,
            section->sh_entsize,
            section->sh_flags & ELF_SHF_ALLOC ? 'A' : '-',
            section->sh_flags & ELF_SHF_WRITE ? 'W' : '-',
            section->sh_flags & ELF_SHF_EXEC_INSTR ? 'X' : '-',
            section->sh_link,
            section->sh_info,
            section->sh_addralign
        );
    }
}

static void debug_elf_program_header(bool title_line, elf32_program_header_t *program) {
    char *ptypes[] = {
        "NULL",
        "LOAD",
        "DYNAMIC",
        "INTERP",
        "NOTE",
        "SHLIB",
        "PHDR"
    };

    if (title_line) {
        klog_debug("  Type     Offset     Virt Addr  Phys Addr  FileSiz    MemSiz     Flg Align");
        //            12345678 0x12345678 0x12345678 0x12345678 0x12345678 0x12345678 123 0x0000
    } else {
        klog_debug("  %-8s 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x %c%c%c 0x%04x",
            program->p_type <= 6 ? ptypes[program->p_type] : "?",
            program->p_offset,
            program->p_vaddr,
            program->p_paddr,
            program->p_filesz,
            program->p_memsz,
            program->p_flags & PF_READ ? 'R' : '-',
            program->p_flags & PF_WRITE ? 'W' : '-',
            program->p_flags & PF_EXEC ? 'X' : '-',
            program->p_align
        );
    }
}

static int debug_file_contents(file_t *file, char *title, uint32_t offset, uint32_t length) {
    int err = vfs_seek(file, (int)offset, SEEK_START);
    if (err < 0)
        return err;
    char *p = kmalloc(length);
    memset(p, 0, length);
    err = vfs_read(file, p, length);
    if (err < 0)
        return err;
    klog_debug(title);
    klog_hex16_debug(p, length, 0);
    kfree(p);
    return SUCCESS;
}

static int debug_elf_file(file_t *file) {
    elf32_header_t *header = kmalloc(sizeof(elf32_header_t));
    int err = vfs_seek(file, 0, SEEK_START);
    if (err < 0)
        return err;
    err = vfs_read(file, (char *)header, sizeof(elf32_header_t));
    if (err != sizeof(elf32_header_t))
        return ERR_READING_FILE;

    klog_debug("ELF header for file \"%s\"", file->path);
    debug_elf_header(header);

    // so now we can load all section headers and all program headers
    int sec_hdr_bytes = header->shnum * header->shentsize;
    int prg_hdr_bytes = header->phnum * header->phentsize;
    char *section_headers = kmalloc(header->shnum * header->shentsize);
    char *program_headers = kmalloc(header->phnum * header->phentsize);

    err = vfs_seek(file, header->shoff, SEEK_START);
    if (err < 0)
        return err;
    err = vfs_read(file, section_headers, sec_hdr_bytes);
    if (err < 0)
        return err;

    err = vfs_seek(file, header->phoff, SEEK_START);
    if (err < 0)
        return err;
    err = vfs_read(file, program_headers, prg_hdr_bytes);
    if (err < 0)
        return err;
    
    char *names_data = NULL;
    if (header->shstrndx != 0) {
        elf32_section_header_t *names_header = (elf32_section_header_t *)(section_headers + (header->shstrndx * header->shentsize));
        names_data = kmalloc(names_header->sh_size);
        err = vfs_seek(file, names_header->sh_offset, SEEK_START);
        if (err < 0) return err;
        err = vfs_read(file, names_data, names_header->sh_size);
        if (err < 0) return err;
        klog_debug("Names from names section");
        klog_hex16_debug(names_data, names_header->sh_size, 0);
    }

    klog_debug("Sections");
    debug_elf_section_header(true, 0, NULL, NULL);
    for (int i = 0; i < header->shnum; i++) {
        elf32_section_header_t *section = (elf32_section_header_t *)(section_headers + (i * header->shentsize));
        debug_elf_section_header(false, i, section, names_data);
    }

    // for (int i = 0; i < header->shnum; i++) {
    //     elf32_section_header_t *section = (elf32_section_header_t *)(section_headers + (i * header->shentsize));
    //     char title[64];
    //     sprintfn(title, sizeof(title), "Section #%d data", i);
    //     debug_file_contents(file, title, section->sh_offset, section->sh_size);
    // }

    klog_debug("Programs");
    debug_elf_program_header(true, NULL);
    for (int i = 0; i < header->phnum; i++) {
        elf32_program_header_t *program = (elf32_program_header_t *)(program_headers + (i * header->phentsize));
        debug_elf_program_header(false, program);
    }

    if (names_data)
        kfree(names_data);
    kfree(section_headers);
    kfree(program_headers);
    kfree(header);

    return SUCCESS;

}


int load_elf_file(file_t *file) {
    klog_trace("load_elf_file(\"%s\")", file->path);

    // in theory we load all the loadable segments,
    // allocated pages, aligned into specific granullarity (e.g. 4, 16)
    // that have been memory mapped to the virtual address the elf requires
    // some segments are RO, some as RW, some are execute
    // there is an entry point address in the header, 
    // that corresponds to the address of the "__start()" method,
    // which is in crt0 and which calls main().
    // the ".symtab" segment contains the names and addresses of methods, 
    // and variables, _very_ useful for debugging or tracing malloc() calls.

    elf32_header_t *header = kmalloc(sizeof(elf32_header_t));
    int err = vfs_seek(file, 0, SEEK_START);
    if (err < 0)
        return err;
    err = vfs_read(file, (char *)header, sizeof(elf32_header_t));
    if (err != sizeof(elf32_header_t))
        return ERR_READING_FILE;

    // we don't support dynamically linked executables for now.
    if (header->type != ELF_TYPE_EXECUTABLE)
        return ERR_NOT_SUPPORTED;

    // so now we can load all section headers and all program headers
    int sec_hdr_bytes = header->shnum * header->shentsize;
    int prg_hdr_bytes = header->phnum * header->phentsize;
    char *section_headers = kmalloc(sec_hdr_bytes);
    char *program_headers = kmalloc(prg_hdr_bytes);

    err = vfs_seek(file, header->shoff, SEEK_START);
    if (err < 0)
        return err;
    err = vfs_read(file, section_headers, sec_hdr_bytes);
    if (err < 0)
        return err;

    err = vfs_seek(file, header->phoff, SEEK_START);
    if (err < 0)
        return err;
    err = vfs_read(file, program_headers, prg_hdr_bytes);
    if (err < 0)
        return err;
    
    // find lowest address
    uint32_t lowest_virtual_address = UINT32_MAX;
    for (int i = 0; i < header->phnum; i++) {
        elf32_program_header_t *program = (elf32_program_header_t *)(program_headers + (i * header->phentsize));
        if (program->p_type != PT_LOAD)
            continue;
        lowest_virtual_address = min(lowest_virtual_address, program->p_vaddr);
    }

    // find highest address, to understand what to allocate
    uint32_t highest_virtual_address = 0;
    for (int i = 0; i < header->phnum; i++) {
        elf32_program_header_t *program = (elf32_program_header_t *)(program_headers + (i * header->phentsize));
        if (program->p_type != PT_LOAD)
            continue;
        uint32_t high_address = program->p_vaddr + program->p_memsz;
        highest_virtual_address = max(highest_virtual_address, high_address);
    }

    // we can add the stack to the size (e.g. 256 KB)
    // Windows allocate 1MB, Linux x86_64 allocates 8MB !!!
    // remember, stack grows downards
    uint32_t stack_end_vaddress = highest_virtual_address;
    uint32_t stack_size = 256 * 1024;
    memset((char *)highest_virtual_address, 0x00, stack_size);
    highest_virtual_address += stack_size;


    // we need to load the segments in the specific virtual address, 
    // therefore we need virtual memory, or risk loading somewhere!
    // let's be risky for now!!!! :-) 
    // otherwise, allocate some pages...    
    debug_elf_program_header(true, NULL);
    for (int i = 0; i < header->phnum; i++) {
        elf32_program_header_t *program = (elf32_program_header_t *)(program_headers + (i * header->phentsize));
        debug_elf_program_header(false, program);
    }
    for (int i = 0; i < header->phnum; i++) {
        elf32_program_header_t *program = (elf32_program_header_t *)(program_headers + (i * header->phentsize));
        if (program->p_type != PT_LOAD)
            continue;

        // allocate and map virtual memory, or use specificed address directly
        // initialize for memory size, but load for file size
        // TODO: allocate memory, implement paging etc.
        klog_trace("loading elf segment from file offset 0x%x (%d bytes) into memory addr 0x%x (%d bytes)", 
            program->p_offset, program->p_filesz,
            program->p_vaddr, program->p_memsz);
        uint8_t *segment_ptr = (uint8_t *)program->p_vaddr;
        memset(segment_ptr, 0, program->p_memsz);
        err = vfs_seek(file, program->p_offset, SEEK_START);
        if (err < 0) return err;
        err = vfs_read(file, segment_ptr, program->p_filesz);
        if (err < 0) return err;
    }

    // then we need to provide a view things for the crt0
    // and jump to the entry point provided by the header.
    // e.g. environment (an array of pointers to "KEY=VALUE" strings, last pointer is NULL)
    // maybe https://pubs.opengroup.org/onlinepubs/9699919799/functions/environ.html


    // create & initialize a process, don't start it yet, optinal association with a tty
    // process_t *create_process(func_ptr entry_point, char *name, uint8_t priority, tty_t *tty);
    // maybe we need to give the process a lot of info, such as the cr0 page
    // the stack location, the jump point, etc.

    uint32_t program_size = highest_virtual_address - lowest_virtual_address;
    klog_debug("ELF \"%s\" loaded address 0x%x..0x%x, total size %d KB", 
        file->path,
        lowest_virtual_address,
        highest_virtual_address,
        program_size / 1024
    );
    klog_debug("Entry point at 0x%x", header->entry);
    klog_debug("Stack top   at 0x%x", highest_virtual_address);
    // klog_hex16_debug((uint8_t *)lowest_virtual_address, program_size - stack_size, lowest_virtual_address);

    tty_t *tty = tty_manager_get_device(1);
    process_t *process = create_process(
        (func_ptr)header->entry,
        file->path,
        (void *)highest_virtual_address,
        PRIORITY_USER_PROGRAM,
        tty
    );

    klog_debug("Starting process PID %d for ELF \"%s\"", process->pid, file->path);
    start_process(process);

    kfree(section_headers);
    kfree(program_headers);
    kfree(header);

    return SUCCESS;
}

