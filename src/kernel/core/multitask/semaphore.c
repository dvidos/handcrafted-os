#include <stddef.h>
#include <stdbool.h>
#include "process.h"
#include "semaphore.h"
#include "scheduler.h"
#include "../memory/kheap.h"
#include "../klog.h"
#include "../string.h"

semaphore_t *create_semaphore(int limit) {
    semaphore_t *semaphore = kalloc(sizeof(semaphore_t));
    memset((char *)semaphore, 0, sizeof(semaphore_t));
    semaphore->limit = limit;
    return semaphore;
}

void acquire_semaphore(semaphore_t *semaphore) {
    lock_scheduler();

    if (semaphore->count < semaphore->limit) {
        semaphore->count++;
        klog("K: process %s acquired semaphore\n", running_process()->name);
        // we got it!
    } else {
        klog("K: process %s getting blocked on semaphore\n", running_process()->name);
        // we cannot acquire, we'll just block until it's free
        semaphore->waiting_processes++;
        block_me(SEMAPHORE, semaphore);
    }

    unlock_scheduler();
}

void release_semaphore(semaphore_t *semaphore) {
    lock_scheduler();


    bool unblocked_a_process = false;
    process_t *target_proc = NULL;
    if (semaphore->waiting_processes > 0) {
        klog("K: semaphore releasing, there are waiting processes\n");
        // if there are processes waiting, let's unblock them now
        target_proc = blocked_list.head;
        while (target_proc != NULL) {
            if (target_proc->state == BLOCKED 
                && target_proc->block_reason == SEMAPHORE 
                && target_proc->block_channel == semaphore
            ) {
                unblock_process(target_proc);
                unblocked_a_process = true;
                break;
            }
            target_proc = target_proc->next;
        }
    }
    if (unblocked_a_process) {
        // somebody was waiting and we liberated them
        // we don't lower the count, they now hold the semaphore
        klog("K: waiting process %s now unblocked to own the semaphore\n", target_proc->name);
        semaphore->waiting_processes--;
    } else {
        // either nobody was waiting, or we did not find them (they may have been killed)
        // lower number as expected
        klog("K: semaphore released by process %s\n", running_process()->name);
        semaphore->count--;
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


