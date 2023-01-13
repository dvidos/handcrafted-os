#ifndef _SETUP_H
#define _SETUP_H

struct testing_framework_setup {
    void *(*malloc)(unsigned size);
    void (*free)(void *address);
    void (*printf)(char *format, ...);
    void (*printf_value)(const char *prefix, const void *value);
    unsigned long (*heap_free)();
};

extern struct testing_framework_setup testing_framework_setup;

void testing_framework_init(
    void *(*memory_allocator)(unsigned size),
    void (*memory_deallocator)(void *address),
    void (*printf_function)(char *format, ...),
    unsigned long (*heap_free_reporter)()
);


#endif
