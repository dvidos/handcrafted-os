
// dummyest thing ever: start at 1MB, give people memory!
static char *free_memory_ptr = (char *)(1024 * 1024);

void *kmalloc(int size) {
    void *p = (free_memory_ptr);
    free_memory_ptr += size;
    return p;
}
