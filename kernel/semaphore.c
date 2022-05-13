#include <stddef.h>
#include "kheap.h"
#include "proclist.h"
#include "process.h"



struct semaphore {
    int limit;
    int count;
    proc_list_t waiting_list;
};
typedef struct semaphore semaphore_t;
typedef struct semaphore mutex_t;



semaphore_t *create_semaphore(int limit) {
    semaphore_t *semaphore = kalloc(sizeof(semaphore_t));
    memset((char *)semaphore, 0, sizeof(semaphore_t));

    semaphore->limit = limit;
    return semaphore;
}

void acquire_semaphore(semaphore_t *semaphore) {
    lock_scheduler();
    if (semaphore->count < semaphore->limit) {
        // we got it
        semaphore->count++;
    } else {
        // we cannot acquire, we'll just block until it's free
        // the running process is not in a list, so we can use its "next" property
        append(&semaphore->waiting_list, running_process());
        block_me(SEMAPHORE);
    }

    unlock_scheduler();
}

void release_semaphore(semaphore_t *semaphore) {
    lock_scheduler();
    if (semaphore->waiting_list.head == NULL) {
        // nobody waits for this, we are good
        semaphore->count--;
    } else {
        // we don't lower the number, we just unblock whoever waits
        // i.e. we would do -1, then they should do +1
        unblock_process(dequeue(&semaphore->waiting_list));
    }

    unlock_scheduler();
}

mutex_t *create_mutex() {
    return (mutex_t *)create_semaphore(1);
}

void acquire_mutex(mutex_t *mutex) {
    acquire_semaphore((semaphore_t *)mutex);
}

void release_mutex(mutex_t *mutex) {
    release_semaphore((semaphore_t *)mutex);
}

