#pragma once


// panic has no dependencies. 
// the actual requirement is to stop the cpu before corruption.
// the display of the message is secondary.
void panic(const char *message);


// if/when a writer is available, we can set it,
// but dependencies still point towards panic, not away from it.
typedef void (panic_writer_func)(const char *message);
void panic_set_writer(panic_writer_func *writer);

