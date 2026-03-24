#include "scheduler.h"
#include "proclist.h"
#include "../multitask.h"
#include "../process/process.h"
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
volatile uint32_t irrelevant_var1;
volatile uint32_t irrelevant_var2;
volatile uint32_t proc_switch_new_esp;
volatile uint32_t irrelevant_var3;
volatile uint32_t irrelevant_var4;
volatile uint32_t proc_switch_tss_address;



/**
 * this method, written in assembly, performs a task switch
 * it takes two pointers to a uint32_t value
 * - first, it pushes a lot of registers on the stack, 
 * - then saves the ESP into the location pointed by the first argument.
 * - then it takes the value pointed by the second argument and sets ESP
 * - then it pops registers in the reverse order.
 * it will return to the caller whose ESP was saved as the second argument
 * 
 * if both pointers point to the same address, no apparent change will happen
 *
 * After the call, the old_esp value will point to the bottom of the saved stack
 * The stack_snapshot structure maps fields to what should be there in memory
 * if we make a stack_snapshot pointer to point to that value, we can see what's pushed
 * If we prepare such a structure, we can create a new task to switch to.
 * The way things are pushed and the stack_snapshot struct must be kept in sync
 */
extern void low_level_context_switch(uint32_t *old_esp_ptr, uint32_t *new_esp_ptr, uint32_t page_directory_address, uint32_t proc_stack_top);


// global variables used in assembly. checked after interrupt handling and make switch if needed.



void lock_scheduler() {
    pushcli();
    switching_postpone_depth++;
}


void unlock_scheduler() {
    switching_postpone_depth--;
    if (switching_postpone_depth == 0) {
        // if there was a need to switch, while postponed,
        // do it before we enable interrupts again
        if (task_switching_pending) {
            task_switching_pending = false;
            schedule();
        }
    }
    popcli();
}


// caller is responsible for locking interrupts before calling us
void schedule() { 

    // allow locking of switching, to allow multiple tasks to be unlbocked
    if (switching_postpone_depth > 0) {
        task_switching_pending = true;
        return;
    }

    if (task_switching_pending)
        panic("In scheduler, while there's already a task switching pending");

    // extract high priority tasks first
    process_t *next = NULL;
    for (int priority = 0; priority < PROCESS_PRIORITY_LEVELS; priority++) {
        next = proclist_dequeue(&ready_lists[priority]);
        if (next != NULL)
            break;
    }

    if (next == NULL)
        return; // nothing to switch to
    
    // if current task is running (as opposed to be blocked or sleeping), put back to the ready list
    process_t *previous = (process_t *)running_proc;
    if (previous->state == RUNNING) {
        previous->state = READY;
        proclist_append(&ready_lists[previous->priority], previous);
    }
    log_debug_fmt("previous:", previous, proc_log_formatter);
    log_debug_fmt("upcoming:", next, proc_log_formatter);


    // before switching, some house keeping
    previous->cpu_ticks_total += (timer_get_uptime_msecs() - previous->cpu_ticks_last);

    running_proc = next;
    running_proc->state = RUNNING;
    reset_switching_time();

    log_trace("scheduler(): switching \"%s\" --> \"%s\", page dir 0x%p", previous->name, next->name, next->memory.page_dir);

    /**
     * -------------------------------------------------------------------
     * completely unintiutive, but immensely important:
     * before and after this call, we are in a different stack frame.
     * the values of all arguments and local variables are different!!!!!
     * for example, after the switch, the "old" becomes whatever was used 
     * to switch out the thing we are going to switch in!!!!
     * so, be careful what the expectations are before and after calling this method.
     * 
     * The Lions book, section 8.9 says that the "swtch()" method (what is one does)
     * does not access any local variables, only global and static ones.
     * Such approach could avoid painful maintenance in the future.
     * -------------------------------------------------------------------
     */
    // low_level_context_switch(
    //     &previous->memory.saved_esp,
    //     (uint32_t *)&running_proc->memory.saved_esp,
    //     (uint32_t)running_proc->memory.page_dir,
    //     running_proc->memory.tss_esp0_value
    // );

    // instead of switching, we just update the global variables.
    proc_switch_needed       = true;
    proc_switch_old_esp_ptr  = (uint32_t)&previous->memory.saved_esp;
    proc_switch_new_cr3      = (uint32_t)running_proc->memory.page_dir;
    proc_switch_new_tss_esp0 = (uint32_t)running_proc->memory.tss_esp0_value;
    proc_switch_new_esp      = (uint32_t)running_proc->memory.saved_esp;
    proc_switch_tss_address  = tss_address;
    irrelevant_var1 = proc_switch_old_esp_ptr + 2;
    irrelevant_var2 = proc_switch_old_esp_ptr + 3;
    irrelevant_var3 = proc_switch_old_esp_ptr + 4;
    irrelevant_var4 = proc_switch_old_esp_ptr + 5;
    log_debug("set switching vars: needed=%d, old_esp_ptr=0x%x, new_cr3=0x%x, new_tss_esp0=0x%x, new_esp=0x%x, tss_addr=0x%x",
        proc_switch_needed,
        proc_switch_old_esp_ptr,
        proc_switch_new_cr3,
        proc_switch_new_tss_esp0,
        proc_switch_new_esp,
        proc_switch_tss_address
    );
    log_debug("set switching vars: var1=%x, var2=%x, var3=%x, var4=%x",
        irrelevant_var1,
        irrelevant_var2,
        irrelevant_var3,
        irrelevant_var4
    );


    running_proc->cpu_ticks_last = timer_get_uptime_msecs();
}

