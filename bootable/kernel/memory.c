#include <stddef.h>



/**
 need to detect available memory
 see also https://wiki.osdev.org/Detecting_Memory_(x86)
 it seems people avoid very low memory (< 1MB)
 there also seems to be a hole around 15M - 16M.
 

 should be able to grab a page (?) of memory to give to a process
 should be able to malloc/free at will


*/



void *memory_start;

void init_memory(void *free_memory_start) {
    memory_start = free_memory_start;
}

/*
char *calloc(int elements, int element_size) {
    return malloc(elements * element_size);
}

char *malloc(size_t size) {

}

char *realloc(char *address, size_t new_size) {
    // expand or shrink the block
    // in place if possible, otherwise copy to new block
}

void free(char *address) {

}
*/
