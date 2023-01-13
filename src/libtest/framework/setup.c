#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "setup.h"


// variable for keeping pointers to memory and display functions
// allows the framework to be used in many environments
struct testing_framework_setup testing_framework_setup;


// so that it can be run from anywhere, lib, kernel or host machine.
void testing_framework_init(
    void *(*memory_allocator)(unsigned size),
    void (*memory_deallocator)(void *address),
    void (*printf_function)(char *format, ...),
    unsigned long (*heap_free_reporter)()
) {
    testing_framework_setup.malloc = memory_allocator;
    testing_framework_setup.free = memory_deallocator;
    testing_framework_setup.printf = printf_function;
    testing_framework_setup.heap_free = heap_free_reporter;
}

