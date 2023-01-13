#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "setup.h"


// variable for keeping pointers to memory and display functions
// allows the framework to be used in many environments
struct testing_framework_setup testing_framework_setup;


static void printf_any_value(const char *prefix, const void *value);


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
    testing_framework_setup.printf_value = printf_any_value;
    testing_framework_setup.heap_free = heap_free_reporter;
}


static void printf_any_value(const char *prefix, const void *value) {
    #define prn(c)    (((c)>=32 && (c)<=127) ? (c) : '.')
    testing_framework_setup.printf("%s int=%d, unsigned=%u, hex=0x%x, str=\"%c%c%c%c...\" (len=%d), hex bytes=%02x %02x %02x %02x...", 
        prefix,
        (unsigned long)value,
        (unsigned long)value,
        (unsigned long)value,
        prn(((char *)value)[0]),
        prn(((char *)value)[1]),
        prn(((char *)value)[2]),
        prn(((char *)value)[3]),
        strlen((char *)value),
        ((char *)value)[0],
        ((char *)value)[1],
        ((char *)value)[2],
        ((char *)value)[3]
    );
    #undef prn
}
