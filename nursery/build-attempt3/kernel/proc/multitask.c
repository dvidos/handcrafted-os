#include "multitask.h"
#include "process/process.h"
#include "procman/proclist.h"
#include "procman/scheduler.h"
#include "../drivers/timer.h"
#include "../drivers/clock.h"
#include "../logger/logger.h"
#include "../memory/kheap.h"
#include "../memory/vmm.h"
#include "../klib/string.h"
#include "../klib/cpu_tools.h"
#include "../utils/panic.h"

MODULE("MULTITASK", LOG_LEVEL_INFO);


#define min(a, b)   ((a) < (b) ? (a) : (b))

static volatile bool process_switching_enabled = false;
uint64_t next_switching_time = 0;
uint64_t next_wake_up_time = 0;




void idle_task() {
    // this is the idle task. this task must not sleep or block, it will not collect terminated tasks.
    // it is the most lazy task in the world...
    while (true) {
        asm("hlt");
    }
}

void init_multitasking() {
    // we should not neglect the original task that has been running since boot
    // this is what we will switch "from" into whatever other task we want to spawn.
    // this way we always have a "from" to switch from...
    initialize_process_lists();

    // our task that will be running has to be marked as RUNNING, to be swapped out
    process_t *idle;
    error_t err = process_v2_create_for_kernel("idle", (uintptr_t)idle_task, PRIORITY_IDLE_TASK, &idle);
    if (err) panic("Error creating the idle task: %s", strerror(err));
    // log_debug_fmt(proc_log_formatter, "idle task:", idle);

    // set to running in order to swap it out
    running_proc = idle;
    running_proc->state = RUNNING;
}

// reports whether multitasking has started
bool multitasking_enabled() {
    return process_switching_enabled;
}

// this will never return
void start_multitasking() {
    log_debug("Starting multitasking");

    // dump_process_table();

    // flag to our interrupt handler that we can start scheduling
    // after a while, the timer will switch us out and will switch something else in.
    process_switching_enabled = true;

    // this to enable the scheduled to switch tasks in a while
    next_switching_time = timer_get_uptime_msecs() + DEFAULT_TASK_TIMESLICE_MSECS;

    idle_task();
}


// called by the timer handler
static void wake_sleeping_tasks() {
    if (blocked_list.head == NULL)
        return;

    // move everything to a temp list, then deal with one task at a time
    // tasks are either put back into the sleeping list, or in the ready list.
    proc_list_t temp_list;
    temp_list.head = blocked_list.head;
    temp_list.tail = blocked_list.tail;
    blocked_list.head = NULL;
    blocked_list.tail = NULL;
    uint64_t now = timer_get_uptime_msecs();

    // update the next wake_up_time
    next_wake_up_time = 0;
    process_t *proc = proclist_dequeue(&temp_list);
    while (proc != NULL) {
        if (proc->block_reason == SLEEPING && proc->wake_up_time > 0 && now >= proc->wake_up_time) {
            // log_trace("process %s ready to run, sleep time expired", proc->name);
            proc->state = READY;
            proc->block_reason = 0;
            proc->block_channel = NULL;
            proclist_prepend(&ready_lists[proc->priority], proc);
        } else {
            proclist_append(&blocked_list, proc);
            next_wake_up_time = (next_wake_up_time == 0)
                ? proc->wake_up_time
                : min(next_wake_up_time, proc->wake_up_time);
        }
        proc = proclist_dequeue(&temp_list);
    }
}


void advice_on_next_wake_up_time(uint64_t proc_wake_up_time) {
    if (next_wake_up_time == 0) {
        next_wake_up_time = proc_wake_up_time;
        return;
    }

    if (proc_wake_up_time < next_wake_up_time) {
        next_wake_up_time = proc_wake_up_time;
        return;
    }
    // else we keep the old wake up that will arrive first
}

void reset_switching_time() {
    next_switching_time = timer_get_uptime_msecs() + DEFAULT_TASK_TIMESLICE_MSECS;
}

// should be called from timer IRQ handler
void multitasking_timer_ticked() {
    if (!process_switching_enabled)
        return;
    
    uint64_t uptime_msecs = timer_get_uptime_msecs();
    if (next_wake_up_time > 0 && uptime_msecs >= next_wake_up_time) {
        wake_sleeping_tasks();
    }
    if (next_switching_time > 0 && uptime_msecs >= next_switching_time) {
        // i think that to be able to switch during IRQ, our first switching must be 
        // done through IRQ, meaning, all the new task stacks should return to the IRQ handler.
        prepare_switch_to_another_process();
    }
}

