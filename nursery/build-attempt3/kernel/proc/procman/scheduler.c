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

// for postponing scheduling
static volatile int switching_postpone_depth = 0;
static volatile bool task_switching_pending = false;


// for the assembly switcher in isr
volatile bool     proc_switch_needed;
volatile uint32_t proc_switch_old_esp_ptr;
volatile uint32_t proc_switch_new_cr3;
volatile uint32_t proc_switch_new_tss_esp0;
volatile uint32_t proc_switch_new_esp;
volatile uint32_t proc_switch_tss_address;





static process_t *find_next_runnable_process() {
    // extract high priority tasks first
    for (int priority = 0; priority < PROCESS_PRIORITY_LEVELS; priority++) {
        process_t *next = proclist_dequeue(&ready_lists[priority]);
        if (next != NULL)
            return next;
    }
    return NULL;
}

// caller is responsible for locking interrupts before calling us
void prepare_switch_to_another_process() {

    // allow locking of switching, to allow multiple tasks to be unlbocked
    if (switching_postpone_depth > 0) {
        task_switching_pending = true;
        return;
    }
    if (task_switching_pending)
        panic("In scheduler, while there's already a task switching pending");
    
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
    ASSERT(next->memory.tss_esp0_value != 0);
    
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
    // log_debug("Raw upcoming trapframe dump");
    // log_debug_hex((void *)next->memory.saved_esp, sizeof(trap_frame_t), 0);

    // NOTE: we are NOT SWITCHING TASKS here!
    // they are switched whenever we return from C back to the assembly isr handler!
    // so, unless we RETURN there, nothing will switch, and interrupts will stay disabled!

    proc_switch_needed       = true;
    proc_switch_old_esp_ptr  = (uint32_t)&previous->memory.saved_esp;
    proc_switch_new_cr3      = (uint32_t)running_proc->memory.page_dir;
    proc_switch_new_tss_esp0 = (uint32_t)running_proc->memory.tss_esp0_value;
    proc_switch_new_esp      = (uint32_t)running_proc->memory.saved_esp;
    proc_switch_tss_address  = tss_address;
    
    log_trace("scheduler(): ISR assmebly shall switch from %s[%d] --> %s[%d]", previous->name, previous->pid, next->name, next->pid);
    //log_debug("switching vars: needed=%d, old_esp_ptr=0x%x, new_cr3=0x%x, new_tss_esp0=0x%x, new_esp=0x%x, tss_addr=0x%x", proc_switch_needed, proc_switch_old_esp_ptr, proc_switch_new_cr3, proc_switch_new_tss_esp0, proc_switch_new_esp, proc_switch_tss_address);
    // log_debug("scheduler(): ISR assmebly shall switch from ESP 0x%x to ESP 0x%x, from CR3 0x%x to CR3 0x%x", previous->memory.saved_esp, running_proc->memory.saved_esp, previous->memory.page_dir, next->memory.page_dir);
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
