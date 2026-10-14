#include "../debugging/kdebug.h"
#include "../include/ctypes.h"
#include "../klib/string.h"
#include "../logger/logger.h"
#include "../proc/process/process.h"
#include "../proc/elf_reader.h"
#include "../memory/virtual/vmm.h"
#include "../memory/physical/physmem.h"
#include "../memory/mem_regions.h"
#include "../memory/mem_region.h"
#include "../memory/kernel_memory_map.h"
#include "../filesys/vfs_api.h"
#include "../filesys/vfs_objects/open_file.h"
#include "../filesys/vfs_objects/inode.h"
#include "../filesys/vfs_objects/mount_table.h"
#include "../drivers/serial.h"

MODULE("SHELL", LOG_LEVEL_TRACE);



#define SHELL_SERIAL_PORT  0

typedef struct known_formatter {
    const char *name;
    const char *descr;
    log_formatter_t *func;
} known_formatter;

typedef struct shell_cmd {
    char *prefix;
    char *description;
    void (*func)(char *rest);
} shell_cmd;

static known_formatter known_formatters[] = {
    { "proc", "Process struct", proc_log_formatter },
    { "if", "Interrupt frame struct", interrupt_frame_log_formatter },
    { "cf", "C frame struct", c_frame_log_formatter },
    // { "of", "open_file_t struct", open_file_log_formatter },
    { "mm", "mem_regions_t struct", mem_regions_formatter },
    { "mr", "mem_regiot_t struct", mem_region_formatter },
    { "pd", "page directory", vmm_pagedir_log_formatter },
    { "elfs", "elf_segment_t struct", elf_segment_formatter },
    { NULL, NULL, NULL }
};

static kernel_memory_map_t *known_map;

void kshell_set_kernel_map_for_debugging(kernel_memory_map_t *map) {
    known_map = map;
}

// --------------------------------------------------------------

static void skip_whitespace(char **cmd_ptr) {
    while (**cmd_ptr == ' ')
        (*cmd_ptr) += 1;
}

static bool finished(char **cmd_ptr) {
    skip_whitespace(cmd_ptr);
    return (**cmd_ptr == 0);
}

static void get_str_argument(char **cmd_ptr, char *buff, int buff_size, const char *default_value) {
    if (finished(cmd_ptr)) {
        sprintfn(buff, buff_size, "%s", default_value);
        return;
    }
    skip_whitespace(cmd_ptr);
    buff[0] = 0;
    int i = 0;
    while (**cmd_ptr != ' ' && **cmd_ptr != '\0' && i < buff_size - 1) {
        buff[i++] = **cmd_ptr;
        (*cmd_ptr) += 1;
    }
    buff[i] = 0;
}

static uint32_t get_num_argument(char **cmd_ptr, uint32_t default_value) {
    if (finished(cmd_ptr))
        return default_value;
    
    char buff[32];
    get_str_argument(cmd_ptr, buff, sizeof(buff), "");
    return (strlen(buff) == 0) ? default_value : atoui(buff);
}

static bool matches(char **cmd_ptr, char *prefix) {
    skip_whitespace(cmd_ptr);
    int len = strlen(prefix);
    if (memcmp(*cmd_ptr, prefix, len) != 0)
        return false;
    
    (*cmd_ptr) += len;
    return true;
}

static void shprintf(const char *fmt, ...) {
    char buff[128];
    va_list args;
    va_start(args, fmt);
    vsprintfn(buff, sizeof(buff), fmt, args);
    va_end(args);
    serial_write(SHELL_SERIAL_PORT, buff);
}

static bool get_cmd_from_user(char *buffer, int buffer_size) {
    while (true) {
        serial_write(SHELL_SERIAL_PORT, "kernel shell > ");
        serial_wait_gets(SHELL_SERIAL_PORT, buffer, buffer_size);
        if (strlen(buffer) == 0) continue;
        if (strcmp(buffer, "exit") == 0) return false;
        return true;
    }
}

// --------------------------------------------------------------

static void shell_hd(char *args) {
    uint32_t addr = get_num_argument(&args, 0);
    uint32_t size = get_num_argument(&args, 16);
    shprintf("Hex dump, addr = 0x%08x, size = %d\n", addr, size);
    log_debug_hex((void *)addr, size, addr);
}

static void shell_addr(char *args) {
    virt_addr_t addr = get_num_argument(&args, 0);
    shprintf("Symbol at %p: %s\n", addr, kdebug_get_symbol(addr));

    // let's see what we can do.
    // kernel memory map,
    // process memory map,
    // if in kernel heap, can we debug?
    // vmm area
    // ?
    bool is_kernel_space = (addr < vmm_get_kernel_area_end());
    bool is_mapping_window = (addr >= vmm_rmw_base_address());
    
    if (is_kernel_space) {
        shprintf("This is a kernel space pointer\n");

        // do the kernel info
        const char *region_name = "n/a";
        bool is_data = false;
        const mem_region_t *region = mem_regions_find_region(&known_map->regions, addr);
        if (region != NULL) {
            region_name = region->name;
            is_data = mem_region_has_data(region);
        }
        shprintf("Kernel region: %s\n", region_name);

        const char *symbol_name = is_kernel_space ? kdebug_get_symbol(addr) : NULL;
        region = mem_regions_find_by_usage(&known_map->regions, REGION_USAGE_HEAP);
        if (region != NULL && mem_region_contains_address(region, addr)) {
            shprintf("Checking heap pointer integrity...\n");
            kcheck((void *)addr, "address");
        }

        // if stack, could we derive which function declared it? using the EBP chain?

        // if data, let's dump some bytes (e.g. 64 around the address)
        if (is_data) {
            shprintf("Dumping data around address...\n");
            log_debug_hex((void *)addr, 64, addr);
        }

    } else if (is_mapping_window) {
        shprintf("Address belongs to Recursive Mapping Window, top 4 MB of memory\n");

    } else if (running_process() != NULL && proc_is_user_proc(running_process())) {
        // proc-defined info
        process_t *proc = running_process();
        bool is_owned_by_proc = false;
        char proc_region_name[64];

        for (int i = 0; i < MAX_PROCESS_ELF_SECTIONS; i++) {
            if (mem_region_contains_address(&proc->memory.elf_sections[i], addr)) {
                is_owned_by_proc = true;
                sprintfn(proc_region_name, sizeof(proc_region_name), "elf_section[%d]", i);
                break;
            }
        }
        if (mem_region_contains_address(&proc->memory.user_heap, addr)) {
            is_owned_by_proc = true;
            strcpy(proc_region_name, "heap");
        }
        if (mem_region_contains_address(&proc->memory.user_stack, addr)) {
            is_owned_by_proc = true;
            strcpy(proc_region_name, "user stack");
        }
        if (mem_region_contains_address(&proc->memory.ring0_stack, addr)) {
            is_owned_by_proc = true;
            strcpy(proc_region_name, "ring0 stack");
        }

        if (is_owned_by_proc) {
            shprintf("Address owned by current process %s[%d]\b", running_process()->name, running_process()->pid);
            shprintf("Owning region: %s\n", proc_region_name);
            shprintf("Dumping data around address...\n");
            log_debug_hex((void *)addr, 64, addr);
        }
    }
}

static void shell_pretty_print(char *args) {
    uint32_t addr = get_num_argument(&args, 0);
    char formatter_name[32];
    get_str_argument(&args, formatter_name, sizeof(formatter_name), "(none)");

    // we need to (a) find formatter, (b) cache formatter for address.
    log_formatter_t *fmt = NULL;
    for (int i = 0; known_formatters[i].name != NULL; i++) {
        if (strcmp(known_formatters[i].name, formatter_name) == 0) {
            fmt = known_formatters[i].func;
            break;
        }
    }
    if (fmt == NULL) {
        shprintf("Formatter '%s' not found, known formatters follow:\n", formatter_name);
        for (int i = 0; known_formatters[i].name != NULL; i++) {
            shprintf("    %-12s %s\n", known_formatters[i].name, known_formatters[i].descr);
        }
        return;
    }

    log_info_fmt(fmt, "| ", (void *)addr);
}

static void shell_fs_info(char *args) {
    shprintf("not implemented yet\n");
}

static void shell_proc_info(char *args) {
    shprintf("not implemented yet\n");
}

static void shell_kmm(char *args) {
    shprintf("not implemented yet\n");
}

static shell_cmd cmds[] = {
    { "exit",     "exits the shell", NULL },
    { "help",     "lists available commands", NULL },
    { "hd",       "hd <addr> [<size>]     hex dumps a portion of memory.", shell_hd },
    { "addr",     "addr <addr>            information about this address", shell_addr },
    { "pp",       "pp <addr> <formatter>  prints object using formatter", shell_pretty_print },
    { "fsinfo",   "fs                     information about the filesystem", shell_fs_info },
    { "procinfo", "proc                   information about the processes", shell_proc_info },
    { "kmm",      "kmm                    print kernel's mem map", shell_kmm },
    // mount table (vfs stuff), pmm, vmm, kernel mem map, process list, specific process, vfs cmd (create, unlink, cp, ls, cat, etc)
};

// ----------------------------------------------------------------------

void kshell() {
    char cmd[128];
    char text[128];
    char *ptr;
    int num_cmds = sizeof(cmds) / sizeof(cmds[0]);

    while (1) {
        if (!get_cmd_from_user(cmd, sizeof(cmd)))
            break;

        if (strcmp(cmd, "help") == 0) {
            for (int i = 0; i < num_cmds; i++)
                shprintf("  %-10s %s\n", cmds[i].prefix, cmds[i].description);
            continue;
        }

        ptr = cmd;
        bool cmd_found = false;
        for (int i = 0; i < num_cmds; i++) {
            if (!matches(&ptr, cmds[i].prefix))
                continue;
            cmd_found = true;
            skip_whitespace(&ptr);
            cmds[i].func(ptr);
            break;
        }

        if (!cmd_found)
            shprintf("cmd '%s' not found, 'help' for available commands\n", cmd);
    }
}
