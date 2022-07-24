#include <stdio.h>



int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    puts("Hello from condemo user program!\n");
    int color = get_screen_color();
    set_screen_color(COLOR_BLUE);
    puts("This would be blue\n");
    set_screen_color(COLOR_GREEN);
    puts("This would be green\n");
    set_screen_color(COLOR_RED);
    puts("This would be red\n");
    set_screen_color(COLOR_CYAN);
    puts("This would be cyan\n");
    set_screen_color(COLOR_MAGENTA);
    puts("This would be purple\n");


    int rows, cols;
    screen_dimensions(&cols, &rows);

    int x, y;
    where_xy(&x, &y);
    goto_xy(cols - 10, 2);
    set_screen_color(COLOR_RED << 4 | COLOR_WHITE);
    puts("Up Here!");
    goto_xy(x, y);

    set_screen_color(color);
    puts("Back to boring colors, good bye!\n");

    return 0;
}


