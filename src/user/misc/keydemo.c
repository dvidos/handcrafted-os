#include <stdio.h>
#include <string.h>


int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    key_event_t e;
    puts("Press any key to test getkey() method, Esc to exit\n");
    while (true) {
        getkey(&e);
        if (e.keycode == KEY_ESCAPE)
            break;

        if (e.ascii != 0) {
            putchar(e.ascii);
        } else if (e.keycode != 0) {
            printf("[0x%x]", e.keycode);
        }
    }

    return 0;
}


