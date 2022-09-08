#include <stdio.h>



int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    int old = get_screen_color();

    for (int bg = 0; bg < 16; bg++) {
        for (int fg = 0; fg < 16; fg++) {
            set_screen_color(bg << 4 | fg);
            printf(" Abc");
        }
        printf(" \n");
    }

    set_screen_color(old);
    return 0;
}


