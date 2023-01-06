#ifndef _RAND_H
#define _RAND_H

#include <ctypes.h>



// not good for cryptographic operations
uint32_t rand();
uint32_t rand_range(uint32_t min, uint32_t max);

void srand(uint64_t seed);
void srand_time();


#endif
