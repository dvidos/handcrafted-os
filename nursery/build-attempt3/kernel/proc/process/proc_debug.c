#include "process.h"
#include "../procman/proclist.h"
#include "../../logger/logger.h"
#include "../../memory/kheap.h"

MODULE("PROC_DBG", LOG_LEVEL_DEBUG);



static char *process_state_names[] = { "READY", "RUNNING", "BLOCKED", "TERMINATED" };
static char *process_block_reason_names[] = { "", "SLEEPING", "SEMAPHORE", "WAIT USER INPUT", "WAIT CHILD EXIT" };



static void dump_process(process_t *proc) {
    log_info("%-4d %-4d %-20s %08x %08x %-10s %-10s %4us", 
        proc->pid,
        proc->parent == NULL ? 0 : proc->parent->pid,
        proc->name, 
        proc->memory.saved_esp, 
        proc->entry_point,
        (char *)process_state_names[(int)proc->state],
        (char *)process_block_reason_names[proc->block_reason],
        (proc->cpu_ticks_total / 1000)
    );
}

static void dump_process_list(proc_list_t *list) {
    process_t *proc = list->head;
    while (proc != NULL) {
        dump_process(proc);
        proc = proc->list_next;
    }
}

void dump_process_table() {
    log_info("Process list:");
    log_info("PID  PPID Name                 ESP      EIP      State      Blck Reasn    CPU");
    dump_process((process_t *)running_process());
    for (int pri = 0; pri < PROCESS_PRIORITY_LEVELS; pri++) {
        dump_process_list(&ready_lists[pri]);
    }
    dump_process_list(&blocked_list);
    dump_process_list(&terminated_list);
}

const char *proc_get_status_name(enum process_state state) {
    switch (state) {
        case READY:
            return "READY";
        case RUNNING:
            return "RUNNING";
        case BLOCKED:
            return "BLOCKED";
        case TERMINATED:
            return "TERM";
        default:
            return "?";
    }
}

const char *proc_get_block_reason_name(enum block_reasons reason) {
    switch (reason) {
        case SLEEPING:
            return "SLEEP";
        case SEMAPHORE:
            return "SEMAPHORE";
        case WAIT_USER_INPUT:
            return "WAIT_KBD";
        case WAIT_CHILD_EXIT:
            return "WAIT CHILD";
        default:
            return "?";
    }
}


static void format_mem_region(log_write_stream_t *stream, char *name, mem_region_t *reg) {
    // |  12345678  0x12345678  0x12345678  1234567890  1234  XXX  XXX  code
    stream->printf(stream, "    %-8s  0x%08x  0x%08x  %10d  %4d  %-3s  %-3s  %s",
        name, 
        reg->address,
        reg->address + reg->size,
        reg->size,
        reg->size / 1024,
        reg->flags & REGION_USER_ACCESSIBLE ? "Usr" : "---",
        reg->flags & REGION_WRITE_ENABLE ? "Wrt" : "---",
        mem_region_usage_name(reg)
    );
}


void dump_bytes(bool direct, uintptr_t address, int how_many) {
    char *buffer = kmalloc(4096);
    if (direct) {
        log_debug("Dumping %d bytes at address 0x%x", how_many, address);
        log_debug_hex((void *)address, how_many, 0);
    } else {
        size_t offset_in_page = address - vmm_round_down(address);
        vmm_physpg_read(vmm_round_down(address), 0, buffer, 4096);
        log_debug("Dumping %d bytes at address 0x%x", how_many, address);
        log_debug_hex(buffer + offset_in_page, how_many, 0);
    }
    kfree(buffer);
}

void proc_log_formatter(log_write_stream_t *stream, va_list args) {
    process_t *proc = va_arg(args, process_t *);

    stream->printf(stream, "Process ptr=0x%x, pid=%d, ppipd=%d, name='%s', priority=%d, is_user=%d", 
        proc,
        proc_get_pid(proc),
        proc_get_ppid(proc),
        proc->name,
        proc->priority,
        proc->is_user ? 1 : 0
    );

    stream->printf(stream, "- Memory: (proc_pd=0x%x, curr_pd=0x%x)", proc->memory.page_dir, vmm_get_current_page_dir());
    stream->printf(stream, "    Region       Address          To        Size    KB  Usr  Wrt  Usage");
    format_mem_region(stream, "kstack", &proc->memory.kernel_stack);
    format_mem_region(stream, "ustack", &proc->memory.user_stack);
    format_mem_region(stream, "uheap", &proc->memory.user_heap);
    if (!mem_region_is_empty(&proc->memory.elf_sections[0])) format_mem_region(stream, "elf #0", &proc->memory.elf_sections[0]);
    if (!mem_region_is_empty(&proc->memory.elf_sections[1])) format_mem_region(stream, "elf #1", &proc->memory.elf_sections[1]);
    if (!mem_region_is_empty(&proc->memory.elf_sections[2])) format_mem_region(stream, "elf #2", &proc->memory.elf_sections[2]);
    if (!mem_region_is_empty(&proc->memory.elf_sections[3])) format_mem_region(stream, "elf #3", &proc->memory.elf_sections[3]);
    
    // trapframe will always be in kernel_stack, therefore always identity mapped
    trap_frame_t *tf = (trap_frame_t *)proc->memory.saved_esp;
    stream->printf(stream, "- Trap frame (saved_esp=0x%08x, tss_esp0=0x%08x)", proc->memory.saved_esp, proc->memory.tss_esp0_value);
    stream->print_fmt(stream, "   ", trap_frame_log_formatter, tf);

    // stream->printf(stream->context, "- Arguments");
    // stream->printf(stream->context, "- Environment");

    stream->printf(stream, "- File descriptors");
    bool handle_found = false;
    for (int i = 0; i < MAX_FILE_HANDLES; i++) {
        if (proc->file_handles[i] == NULL)  
            continue;
        
        // this prefix should be added to the existing one
        char prefix[16];
        sprintfn(prefix, sizeof(prefix), "  [%d] ", i);
        log_debug_fmt(open_files.formatter, prefix, proc->file_handles[i]);
    }
    if (!handle_found)
        stream->printf(stream, "    (none found)");

    stream->printf(stream, "- Memory mapping");
    stream->print_fmt(stream, "  ", vmm_pagedir_log_formatter, proc->memory.page_dir);
}
