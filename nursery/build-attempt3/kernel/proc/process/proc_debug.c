#include "process.h"
#include "../procman/proclist.h"
#include "../../logger/logger.h"

MODULE("PROC_DBG", LOG_LEVEL_TRACE);



static char *process_state_names[] = { "READY", "RUNNING", "BLOCKED", "TERMINATED" };
static char *process_block_reason_names[] = { "", "SLEEPING", "SEMAPHORE", "WAIT USER INPUT", "WAIT CHILD EXIT" };



static void dump_process(process_t *proc) {
    log_info("%-4d %-4d %-20s %08x %08x %-10s %-10s %4us", 
        proc->pid,
        proc->parent == NULL ? 0 : proc->parent->pid,
        proc->name, 
        proc->memory.execution.stack_pointer, 
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
    stream->printf(stream->context, "    %-8s  0x%08x  0x%08x  %10d  %4d  %-3s  %-3s  %s",
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


void proc_log_formatter(log_write_stream_t *stream, va_list args) {
    process_t *proc = va_arg(args, process_t *);

    stream->printf(stream->context, "Process pid=%d, ppipd=%d, name='%s', priority=%d, flags=0x%x", 
        proc_get_pid(proc),
        proc_get_ppid(proc),
        proc->name,
        proc->priority,
        proc->flags
    );

    stream->printf(stream->context, "- Memory: (page_dir=0x%x)", proc->memory.page_dir);
    stream->printf(stream->context, "    Region       Address          To        Size    KB  Usr  Wrt  Usage");
    format_mem_region(stream, "stack", &proc->memory.stack);
    format_mem_region(stream, "elf #0", &proc->memory.elf_sections[0]);
    format_mem_region(stream, "elf #1", &proc->memory.elf_sections[1]);
    format_mem_region(stream, "elf #2", &proc->memory.elf_sections[2]);
    format_mem_region(stream, "elf #3", &proc->memory.elf_sections[3]);
    format_mem_region(stream, "heap", &proc->memory.heap);
    
    // we should show other things as well, such as arguments, environment, open files etc.
}