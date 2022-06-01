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
void konsole() {
    
    readline_t *rl = init_readline("dv @ konsole $ ");

    readline_add_keyword(rl, "hheap");
    readline_add_keyword(rl, "mdump");
    readline_add_keyword(rl, "inb");
    readline_add_keyword(rl, "inw");
    readline_add_keyword(rl, "inl");
    readline_add_keyword(rl, "outb");
    readline_add_keyword(rl, "outw");
    readline_add_keyword(rl, "outl");
    readline_add_keyword(rl, "mpeek");

    while (true) {
        char *line = readline(rl);

        tty_write("You've entered: \"");
        tty_write(line);
        tty_write("\"\n");
    }

}