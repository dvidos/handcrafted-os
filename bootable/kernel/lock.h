#ifndef _LOCK_H
#define _LOCK_H

typedef volatile int lock_t;

void acquire(lock_t* lock);
void release(lock_t *lock);


#endif
