#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

// semaphore for more than one holders
typedef struct semaphore semaphore_t;

// allocates, initializes and returns a semaphore - remember to free() when done
semaphore_t *create_semaphore(int limit);

// blocks until acquired
void acquire_semaphore(semaphore_t *semaphore);

// releases, possibly wakes any waiting processes
void release_semaphore(semaphore_t *semaphore);



// mutex is same as semaphore, but only one process can hold it
typedef struct semaphore mutex_t;

// allocates, initializes and returns a mutex - remember to free() when done
mutex_t *create_mutex();

// blocks until mutex acquired
void acquire_mutex(mutex_t *mutex);

// releases mutex 
void release_mutex(mutex_t *mutex);



// we don't know who "owns" the semaphore at each time,
// e.g. when releasing() we will wake up a waiting process at random
// so the only unblocked process owns the semaphore.
// we could have a process pointer, but it may be more than one owners, if limit > 1
struct semaphore {
    int limit;
    int count;
    int waiting_processes; // how many are waiting to release it
};


#endif

