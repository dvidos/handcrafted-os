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
    while (true) {
        // get_command("> ");
        // run_command();
    }
}




void konsole_v2() {
    // initialize
    init_readline("dv @ konsole $ ");

    // then
    while (true) {
        char *line = readline();
        printf("You've entered: \"%s\"\n", line);
    }

}