#ifdef HOSTED_ENV

#include <stdlib.h>
void *kmalloc(int size) { return malloc(size); }
void kfree(void *ptr) { free(ptr); }

#else

// dummyest thing ever: start at 1MB, give people memory!
static char *free_memory_ptr = (char *)(1024 * 1024);

void *kmalloc(int size) {
    void *p = (free_memory_ptr);
    free_memory_ptr += size;
    return p;
}

void kfree(void *ptr) {
    // nothing for now
}

#endif