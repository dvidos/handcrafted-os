#ifndef _LOCK_H
#define _LOCK_H

typedef volatile int lock_t;

void mutex_acquire(lock_t* lock);
void mutex_release(lock_t *lock);


#endif
