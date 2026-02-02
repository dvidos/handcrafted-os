#include "panic.h"

static panic_writer_func *_panic_writer = 0;

void panic(const char *message) {
    asm("cli");

    if (_panic_writer) {
        _panic_writer("\nKernel panic:\n");
        _panic_writer(message);
    }

    for(;;)
        asm("hlt");
}

void panic_set_writer(panic_writer_func *writer) {
    _panic_writer = writer;
}
