#include <lock.h>



void acquire(lock_t* lock) {
    // this has to be atomic
    while (!__sync_bool_compare_and_swap(lock, 0, 1)) {
        asm("pause");
    }
}

void release(lock_t *lock) {
    *lock = 0;
}

