#include <stddef.h>
#include <stdbool.h>
#include <multitask/scheduler.h>
#include <multitask/process.h>
#include <multitask/proclist.h>
#include <cpu.h>
#include <drivers/timer.h>
#include <klog.h>



volatile int switching_postpone_depth = 0;
volatile bool task_switching_pending = false;
process_t *running_proc = NULL;
proc_list_t ready_lists[PROCESS_PRIORITY_LEVELS];
proc_list_t blocked_list;
proc_list_t terminated_list;
uint64_t next_switching_time = 0;
uint64_t next_wake_up_time = 0;


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
extern void low_level_context_switch(uint32_t *old_esp_ptr, uint32_t *new_esp_ptr);


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

    // extract high priority tasks first
    process_t *next = NULL;
    for (int priority = 0; priority < PROCESS_PRIORITY_LEVELS; priority++) {
        next = dequeue(&ready_lists[priority]);
        if (next != NULL)
            break;
    }

    if (next == NULL)
        return; // nothing to switch to
    
    // if current task is running (as opposed to be blocked or sleeping), put back to the ready list
    process_t *previous = (process_t *)running_proc;
    if (previous->state == RUNNING) {
        previous->state = READY;
        append(&ready_lists[previous->priority], previous);
    }

    // before switching, some house keeping
    previous->cpu_ticks_total += (timer_get_uptime_msecs() - previous->cpu_ticks_last);

    // mark the new running proc, "next" variable will have a different value afterwards
    running_proc = next;
    running_proc->state = RUNNING;
    next_switching_time = timer_get_uptime_msecs() + DEFAULT_TASK_TIMESLICE_MSECS;

    klog_trace("scheduler(): switching \"%s\" --> \"%s\"", previous->name, next->name);
    
    /**
     * -------------------------------------------------------------------
     * completely unintiutive, but immensely important:
     * before and after this call, we are in a different stack frame.
     * the values of all arguments and local variables are different!!!!!
     * for example, after the switch, the "old" becomes whatever was used 
     * to switch out the thing we are going to switch in!!!!
     * so, be careful what the expectations are before and after calling this method.
     * -------------------------------------------------------------------
     */
    low_level_context_switch(
        &previous->esp,
        (uint32_t *)&running_proc->esp
    );

    running_proc->cpu_ticks_last = timer_get_uptime_msecs();
}

