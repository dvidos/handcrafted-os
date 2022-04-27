#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "screen.h"
#include "string.h"

 /*
 processes created by kernel, shell, user running programs, etc.
 main mechanism is fork().
 fork() shares the same code, file descriptors, all.
 the data segment is copied, but not shared, different address space is created
 This is separate than exec(), so that the shell child has the opportunity fo redirect stdin/out/err.
 process termination is through either termination of program (compiler to call exit()),
 premature voluntary exiting, being killed or signalled without handler, bugs etc.
 any signal sent to a process, is also sent to all its children and so on.
 seems like kernel will create init, the first process, for it to execute /etc/rc.
 after /etc/rc, it looks at /etc/ttytab, and forks as many gettty processes.
 they wait for user name, execute the login process on it, if successful, they run user's shell.

 processes states can be: running, ready (runnable), blocked.
 processes can block either by calling block(), by reading input from a pipe or terminal
 when no data is available,
 processes toggle between running and ready by the scheduler
 they become unblocked when the event they're waiting on has happened.

 it appears that an interrupt may cause the disk task to run, to handle the relevant disk interrupt
 that's the pre-emptive nature of interrrupt driven systems.
 the sleeping disk task is because it is blocked, waiting for this interrupt.
 after being woken, it handles the event, then goes back to sleep, waiting another interrupt.

 threads is another whole chapter we want to support. threads are not processes,
 they all share the code and memory space with the other threads of the process.
 threads are children of processes, many attributes come from them (e.g. PID),
 but they have their own: instruction and stack pointer, regs, state.
 we should push threads implementation later in the progress, because of complexity.


 */

#define STATE_READY     0
#define STATE_RUNNING   1
#define STATE_BLOCKED   2

#define PRIORITY_HIGH   2
#define PRIORITY_MID    1
#define PRIORITY_LOW    0

#define NUM_PRIORITIES  3    // 0=highest, n=lowest
#define NUM_PROCESSES   128


typedef uint16_t pid_t;

struct process_struct {
    pid_t pid;
    pid_t parent_pid;
    uint8_t priority: 2;
    uint8_t state: 2;
    uint16_t running_ticks_left; // each tick a millisecond, usual values 20-50 msecs
    uint16_t waiting_mask;
    char *command_line;
    struct process_struct *next;
} __attribute__((packed));
typedef struct process_struct process_t;

struct process_list_struct {
    process_t *head;
    process_t *tail;
    uint16_t count;
};
typedef struct process_list_struct process_list_t;

bool _list_empty(process_list_t *list);
int _process_index(process_list_t *list, process_t *target);
process_t *_get_process(process_list_t *list, int index);
void _append_process(process_list_t *list, process_t *proc);
process_t *_dequeue_process(process_list_t *list);

// where processes are stored
static process_t processes[NUM_PROCESSES];

// the currently running process
process_t *currently_running_process;

// lists of runnable processes, by priority
process_list_t ready_processes[NUM_PRIORITIES];

// list of blocked processes
process_list_t blocked_processes;



void init_processes() {
    memset((char *)processes, 0, sizeof(processes));
    memset((char *)ready_processes, 0, sizeof(ready_processes));
    memset((char *)&blocked_processes, 0, sizeof(blocked_processes));
    currently_running_process = NULL;
}

void schedule_next_process() {
    // the previously running process either exhausted the ticks or was Blocked
    // find another candidate and run it.
    // don't update current process, caller should have updated it
    // higher priority lists must have precedence

    // useful times to reschedule:
    // - when a process has exited
    // - when a process is blocked on IO
    // - when a process has been created (re-evaluate priorities)
    // - when an IO interrupt happens (maybe unblocks a process?)
    // - when a clock interrupt happens (time sharing)

    // if no process exists (everything waits on something) run the idle process


    // find next process to run
    process_t *target = NULL;
    for (int priority = 0; priority < NUM_PRIORITIES; priority++) {
        if (!_list_empty(&ready_processes[priority])) {
            target = _dequeue_process(&ready_processes[priority]);
            break;
        }
    }

    if (target != NULL) {
        currently_running_process = target;
        currently_running_process->state = STATE_RUNNING;
        currently_running_process->running_ticks_left = 30;
        // what else?
    }
}

void timer_ticked() {
    if (currently_running_process == NULL)
        return;

    currently_running_process->running_ticks_left--;
    if (currently_running_process->running_ticks_left > 0)
        return;

    // this will get paused, find another one to schedule.
    currently_running_process->state = STATE_READY;
    // we should remove it from current queue
    schedule_next_process();
}

void current_process_blocked(uint16_t waiting_mask) {
    // current process blocked, schedule another one.
    currently_running_process->state = STATE_BLOCKED;
    currently_running_process->waiting_mask = waiting_mask;
    schedule_next_process();
}

void start_new_process() {
    // we shall need to start /init/rc as PID 1.
    // how shall we do this?
}

void print_task_table() {
    //      12345 12345 High Running Wait 12345678901234567890
    printf("  PID  PPID Prio State   0x00 Command line - Name");
    for (int i = 0; i < NUM_PROCESSES; i++) {

        if (!processes[0].pid)
            continue;

        printf("%5d %5d %4d %7s %20s",
            processes[i].pid,
            processes[i].parent_pid,
            processes[i].priority,
            processes[i].state == STATE_RUNNING
                ? "Running"
                : (processes[i].state == STATE_READY
                    ? "Ready"
                    : "Blocked"
                ),
            0, // waiting mask
            processes[i].command_line
        );
    }
}



bool _list_empty(process_list_t *list) {
    // opportunity to detect bad data
    return (list->head == NULL && list->tail == NULL && list->count == 0);
}

int _process_index(process_list_t *list, process_t *target) {
    process_t *node = list->head;
    int index = 0;
    while (node != NULL) {
        if (node == target)
            return index;
        node = node->next;
        index++;
    }
    return -1;
}

process_t *_get_process(process_list_t *list, int index) {
    if (index < 0)
        return NULL;
    process_t *node = list->head;
    while (index > 0 && node != NULL) {
        node = node->next;
        index--;
    }
    return node;
}

void _append_process(process_list_t *list, process_t *proc) {
    // this has to work on O(1) time
    if (list->head == NULL && list->tail == NULL && list->count == 0) {
        list->head = proc;
        list->tail = proc;
        list->count = 1;
    } else {
        list->tail->next = proc;
        proc->next = NULL;
        list->count++;
    }
}

process_t *_dequeue_process(process_list_t *list) {
    // this has to work on O(1) time
    process_t *proc;
    if (list->head == NULL && list->tail == NULL && list->count == 0) {
        return NULL;
    } else {
        proc = list->head;
        list->head = proc->next;
        list->count--;
        if (list->head == NULL)
            list->tail = NULL;
        proc->next = NULL;
        return proc;
    }
}
