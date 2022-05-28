#include <stdbool.h>
#include <stdint.h>
#include "../string.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"
#include "../cpu.h"
#include "../multiboot.h"
#include "../drivers/clock.h"
#include "../memory/kheap.h"
#include "../memory/physmem.h"
#include "readline.h"

/**
 * konsole is an interactive shell like thing,
 * that runs in kernel space and allows the user to play
 * (in a dangerous way) with the machine.
 */
void konsole(tty_t *tty) {
    // initialize
    init_readline("dv @ konsole $ ", tty);

    readline_add_keyword("hheap");
    readline_add_keyword("mdump");
    readline_add_keyword("inb");
    readline_add_keyword("inw");
    readline_add_keyword("inl");
    readline_add_keyword("outb");
    readline_add_keyword("outw");
    readline_add_keyword("outl");
    readline_add_keyword("mpeek");

    // then
    while (true) {
        char *line = readline();

        tty_write(tty, "You've entered: \"");
        tty_write(tty, line);
        tty_write(tty, "\"\n");
    }

}