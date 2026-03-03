#ifndef _SCHEDULER_H
#define _SCHEDULER_H



// use this pair to lock/unlock scheduler
void lock_scheduler();
void unlock_scheduler();

// caller is responsible to lock/unlock, before calling schedule()
void schedule();



#endif
