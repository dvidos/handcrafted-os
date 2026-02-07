#include "mutex.h"



void mutex_acquire(lock_t* lock) {
    // this has to be atomic
    while (!__sync_bool_compare_and_swap(lock, 0, 1)) {
        asm("pause");
    }
}

void mutex_release(lock_t *lock) {
    *lock = 0;
}

