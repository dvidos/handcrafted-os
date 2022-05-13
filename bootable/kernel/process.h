#ifndef _PROCESS_H
#define PROCESS_H



// entry points from kernel and timer IRQ
void init_multitasking();          // call this before setting up tasks
void start_multitasking();         // this never returns
void multitasking_timer_ticked();  // this expected to be called from IRQ handler

// utility tools
void dump_process_table();

// actions to manipulate tasks
typedef struct process process_t;
typedef void (* func_ptr)();
process_t *create_process(func_ptr entry_point, char *name);
void start_process(process_t *process);

// actions that a running task can use
void block_me(int reason);
void sleep_me_for(int milliseconds);
void terminate_me();





#endif