#include <multitask/multitask.h>
#include <multitask/process.h>
#include <multitask/scheduler.h>
#include <drivers/timer.h>
#include <drivers/clock.h>
#include <klog.h>
#include <memory/kheap.h>
#include <memory/virtmem.h>
#include <klib/string.h>



#define min(a, b)   ((a) < (b) ? (a) : (b))

volatile bool process_switching_enabled = false;



void init_multitasking() {
    // we should not neglect the original task that has been running since boot
    // this is what we will switch "from" into whatever other task we want to spawn.
    // this way we always have a "from" to switch from...
    memset((char *)&ready_lists, 0, sizeof(ready_lists));
    memset((char *)&blocked_list, 0, sizeof(blocked_list));
    memset((char *)&terminated_list, 0, sizeof(terminated_list));

    // our task that will be running has to be marked as RUNNING, to be swapped out
    char *idle_stack = kmalloc(1024);
    process_t *idle = create_process(NULL, "Idle", idle_stack + 1024, PROCESS_PRIORITY_LEVELS - 1, NULL, 0);
    running_proc = idle;
    running_proc->state = RUNNING; // set to running in order to swap it

    // idle task is by definition the lowest priority
    append(&ready_lists[idle->priority], idle);
}


// this will never return
void start_multitasking() {
    klog_debug("Starting multitasking");

    // flag to our interrupt handler that we can start scheduling
    // after a while, the timer will switch us out and will switch something else in.
    process_switching_enabled = true;

    // we shall become the idle task.
    // this task must not sleep or block
    while (true) {
        
        // maybe not ideal for an idle task, 
        // but maybe we can use it for some housekeeping
        while (terminated_list.head != NULL) {
            process_t *proc = dequeue(&terminated_list);
            klog_trace("idle task cleaning up terminated process %s", proc->name);
            // clean up things here or in a function
            // TODO: better cleanup, see what exec() creates
            // if (proc->stack_buffer != NULL)
            //     kfree(proc->stack_buffer);
            if (proc->page_directory != NULL)
                destroy_page_directory(proc->page_directory);
            if (proc->executable_to_load != NULL)
                kfree(proc->executable_to_load);
            kfree(proc);
        }
        
        asm("hlt");
    }
}


// called by the timer handler
static void wake_sleeping_tasks() {
    if (blocked_list.head == NULL)
        return;
    lock_scheduler();

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
    process_t *proc = dequeue(&temp_list);
    while (proc != NULL) {
        if (proc->block_reason == SLEEPING && proc->wake_up_time > 0 && now >= proc->wake_up_time) {
            // klog_trace("process %s ready to run, sleep time expired", proc->name);
            proc->state = READY;
            proc->block_reason = 0;
            proc->block_channel = NULL;
            prepend(&ready_lists[proc->priority], proc);
        } else {
            append(&blocked_list, proc);
            next_wake_up_time = (next_wake_up_time == 0)
                ? proc->wake_up_time
                : min(next_wake_up_time, proc->wake_up_time);
        }
        proc = dequeue(&temp_list);
    }
    
    unlock_scheduler();
}



// should be called from timer IRQ handler
void multitasking_timer_ticked() {
    if (!process_switching_enabled)
        return;
    
    lock_scheduler();
    uint64_t uptime_msecs = timer_get_uptime_msecs();
    if (next_wake_up_time > 0 && uptime_msecs >= next_wake_up_time) {
        wake_sleeping_tasks();
    }
    if (next_switching_time > 0 && uptime_msecs >= next_switching_time) {
        // i think that to be able to switch during IRQ, our first switching must be 
        // done through IRQ, meaning, all the new task stacks should return to the IRQ handler.
        schedule();
    }
    unlock_scheduler();
}
