#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"



bool edit_command_line(char first_char, char *cmd_line) {
    int cmd_len = 0;
    key_event_t event;
    bool accepted = false;

    int rows;
    screen_dimensions(NULL, &rows);

    cmd_line[0] = first_char;
    cmd_line[1] = '\0';
    while (true) {
        cmd_len = strlen(cmd_line);
        goto_xy(0, rows);
        printf("%-79s", cmd_line);
        goto_xy(cmd_len, rows - 1);

        getkey(&event);
        if (event.keycode == KEY_ENTER) {
            accepted = true;
            break;
        } else if (event.keycode == KEY_ESCAPE) {
            break;
        } else if (event.keycode == KEY_BACKSPACE) {
            if (cmd_len > 0)
                cmd_line[--cmd_len] = '\0';
            if (cmd_len == 0)
                break;
        } else if (event.ascii != 0) {
            if (cmd_len < 80) {
                cmd_line[cmd_len++] = first_char;
                cmd_line[cmd_len] = '\0';
            }
        }
    }

    return accepted;
}

void execute_command_line(char *cmd_line, buffer_t *buffer, bool *should_quit) {
    printf("Executing command [%s]\n", cmd_line);
    // should probably do it char by char, but for now...
    if (strcmp(cmd_line, ":q") == 0) {
        *should_quit = true;
    }
    /*
        After ':' command:
        w - write file
        w! - force write
        q - quit, warn if outstanding changes
        q! - force quit, discard changes
        e<filename> - edit file
        e! - re-edit, discarding changes
        e+<filename> - edit file, starting at EOF
        w<name> - write specific name, warn if file exists
        w!<name> - overwrite file 
        sh - run shell
        !<cmd> - run command in shell
        n - edit next file in arglist
    */
}