#include "process.h"
#include "../procman/proclist.h"
#include "../../logger/logger.h"
#include "../../utils/assert.h"
#include "../../memory/kheap.h"

MODULE("PROC_DBG", LOG_LEVEL_DEBUG);



static char *process_state_names[] = { "READY", "RUNNING", "BLOCKED", "TERMINATED" };
static char *process_block_reason_names[] = { "", "SLEEPING", "SEMAPHORE", "WAIT USER INPUT", "WAIT CHILD EXIT" };



static void dump_process(const char *prefix, process_t *proc) {
    if (proc == NULL) {
        log_info("%-14s (null)", prefix);
        return;
    }

    log_info("%-14s %-4d %-4d %-20s %08x %-10s %-10s", 
        prefix,
        proc->pid,
        proc->parent == NULL ? 0 : proc->parent->pid,
        proc->name, 
        proc->memory.saved_esp, 
        (char *)process_state_names[(int)proc->state],
        (char *)process_block_reason_names[proc->block_reason]
    );
}

static void dump_process_list(const char *prefix, proc_list_t *list) {
    process_t *proc = list->head;
    while (proc != NULL) {
        dump_process(prefix, proc);
        proc = proc->list_next;
    }
}

void dump_process_table() {
    char prefix[16];

    log_info("Process list:");
    log_info("Queue          PID  PPID Name                 ESP      State      Blck Reasn    ");
    dump_process("Running", (process_t *)running_proc);
    for (int pri = 0; pri < PROCESS_PRIORITY_LEVELS; pri++) {
        sprintfn(prefix, sizeof(prefix), "Prio-%d", pri);
        dump_process_list(prefix, &ready_lists[pri]);
    }
    dump_process_list("Blocked", &blocked_list);
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
        case WAIT_ANY_CHILD:
            return "WAIT_ANY_CHILD";
        case WAIT_SPEC_CHILD:
            return "WAIT_SPEC_CHILD";
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

    stream->printf(stream, "Process ptr=0x%x, pid=%d, ppipd=%d, name='%s', priority=%d, is_user=%d, state=%d(%s), blocking=%d(%s)", 
        proc,
        proc_get_pid(proc),
        proc_get_ppid(proc),
        proc->name,
        proc->priority,
        proc->is_user ? 1 : 0,
        proc->state, str_process_state(proc->state),
        proc->block_reason, str_block_reason(proc->block_reason)
    );

    stream->printf(stream, "- Lineage: this=%d parent=%d", proc->pid, proc->parent == NULL ? -1 : proc->parent->pid);
    for (process_t *child = proc->children_list; child != NULL; child = child->next_child) {
        stream->printf(stream, "           child=%d", child->pid);
        ASSERT(child->parent == proc);
    }

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
    c_frame_t *cframe = proc_get_c_frame(proc);
    stream->printf(stream, "- C-frame frame (saved_esp=0x%08x)", proc->memory.saved_esp);
    stream->print_fmt(stream, "   ", c_frame_log_formatter, cframe);
    interrupt_frame_t *iframe = proc_get_interrupt_frame(proc);
    stream->printf(stream, "- Interrupt frame (tss_esp0=0x%08x)", proc->memory.tss_esp0_value);
    stream->print_fmt(stream, "   ", interrupt_frame_log_formatter, iframe);

    // stream->printf(stream->context, "- Arguments");
    // stream->printf(stream->context, "- Environment");

    stream->printf(stream, "- File descriptors");
    bool one_found = false;
    for (int i = 0; i < MAX_FILE_HANDLES; i++) {
        if (proc->file_handles[i] == NULL)  
            continue;
        
        char prefix[16];
        sprintfn(prefix, sizeof(prefix), "    [%d]", i);
        stream->print_fmt(stream, prefix, open_files.formatter, proc->file_handles[i]);
        one_found = true;
    }
    if (!one_found)
        stream->printf(stream, "    (none found)");

    stream->printf(stream, "- Memory mapping (pd=0x%08x)", proc->memory.page_dir);
    stream->print_fmt(stream, "   ", vmm_pagedir_log_formatter, proc->memory.page_dir);
}
