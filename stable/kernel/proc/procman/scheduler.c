#include "scheduler.h"
#include "proclist.h"
#include "../multitask.h"
#include "../process/process.h"
#include "../../utils/assert.h"
#include "../../drivers/timer.h"
#include "../../arch/cpu.h"
#include "../../arch/gdt.h"
#include "../../utils/panic.h"
#include "../../logger/logger.h"

MODULE("SCHED", LOG_LEVEL_DEBUG);

// defined in isr.asm, swaps c_frame_t frames.
extern void switch_inside_c_function(uint32_t *old_esp_ptr, uint32_t new_esp, uint32_t new_cr3, uint32_t new_esp0);


static process_t *find_next_runnable_process() {
    // extract high priority tasks first
    for (int priority = 0; priority < PROCESS_PRIORITY_LEVELS; priority++) {
        process_t *next = proclist_dequeue(&ready_lists[priority]);
        if (next != NULL)
            return next;
    }
    return NULL;
}

void schedule_another_process() {

    process_t *next = find_next_runnable_process();
    if (next == NULL && running_proc->state != RUNNING)
        panic("no ready processes found, running process not running either");

    if (next == NULL) {
        // log_trace("no ready processes found, staying with %s[%d]", running_proc->name, running_proc->pid);
        reset_switching_time();
        return;
    }

    ASSERT(next->state == READY);
    ASSERT(next->memory.page_dir != 0);
    ASSERT(next->memory.ring0_stack_top != 0);
    
    // if current task is running (as opposed to be blocked or sleeping), put back to the ready list
    process_t *previous = (process_t *)running_proc;
    if (previous->state == RUNNING) {
        previous->state = READY;
        proclist_append(&ready_lists[previous->priority], previous);
    }
    running_proc = next;
    running_proc->state = RUNNING;
    reset_switching_time();
    
    // log_debug_fmt(proc_log_formatter, "previous:", previous);
    // log_debug_fmt(proc_log_formatter, "upcoming:", next);

    log_trace("scheduler(): switching from %s[%d] --> %s[%d]", previous->name, previous->pid, next->name, next->pid);

    switch_inside_c_function(
        &previous->memory.ring0_esp,
        running_proc->memory.ring0_esp,
        running_proc->memory.page_dir,
        running_proc->memory.ring0_stack_top
    );

    // we are now in a different stack / process
}


// useful for call from assembly, "push ESP", call this, "add ESP,4"
void debug_stack_contents(uint32_t esp_value) {
    log_debug("Stack dump follows, esp_value=0x%x", esp_value);
    log_debug_hex((void *)esp_value, 256, esp_value);
}

// useful for call from assembly, "push ESP", call this, "add ESP,4"
void debug_one_dword(uint32_t value) {
    log_debug("the value is 0x%x", value);
}
