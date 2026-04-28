#include "panic.h"
#include "../klib/string.h"
#include "../klib/kdebug.h"

static panic_writer_func *_panic_writer = 0;

void panic(const char *fmt, ...) {

    asm("cli");

    if (_panic_writer) {

        va_list vl;
        char buffer[256];
        
        va_start(vl, fmt);
        vsprintfn(buffer, sizeof(buffer), fmt, vl);
        va_end(vl);

        _panic_writer("\nKernel panic: ");
        _panic_writer(buffer);
        _panic_writer("\n");
    }

    kdebug_backtrace();
    
    for(;;)
        asm("hlt");
}

void panic_set_writer(panic_writer_func *writer) {
    _panic_writer = writer;
}
