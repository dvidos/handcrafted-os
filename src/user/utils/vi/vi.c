#include <stdio.h>
#include <keyboard.h>
#include "buffer.h"
#include "viewport.h"


#define MODE_COMMAND         0
#define MODE_INSERT          1
#define MODE_REPLACE_ONE     2
#define MODE_REPLACE_CONT    3
#define MODE_VISUAL_CHARS    4
#define MODE_VISUAL_LINES    5


#define COMMAND_STARTING_CHARS   ":/?!"

int mode = 0;
int finished = 0;
int count = 1;
char command_line[80 + 1] = {0,}; 
char operator = 0;


// draw the footer line
void draw_footer() {
    gotoxy(0, screen_rows() - 1);
    if (mode == MODE_COMMAND) {
        printf("%79s", " ");
    } else if (mode == MODE_INSERT) {
        printf("--- INSERT ---      %d,%d     %d%%", row, col, 
            total_rows == 0 ? 0 : first_visible_row / total_rows);
    }
}

// draw the whole screen anew
void draw_screen() {
    for (int row = 0; row < screen_rows() - 1; row++) {
        gotoxy(0, row);
        if (row < total_rows)
            printf("%-80s", "");
        else
            printf("%-80s", "~");
    }
    // now go where we are supposed to be
    gotoxy(col - first_visible_col, row - first_visible_row);
}


void change_mode(int new_mode) {
    mode = new_mode;
    operator = 0;
    count = 0;
    draw_footer();
}

// run the command line (ex mode)
void execute_command_line() {
    printf("Executing command [%s]\n", command);
    // should probably do it char by char, but for now...
    if (strcmp(command, ":q") == 0) {
        finished = true;
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

// edit the command line, first char can be ':' or '/'
bool edit_command_line(char first_char) {
    int cmd_len = 0;
    key_event_t event;
    bool accepted = false;

    command[0] = first_char;
    command[1] = '\0';
    while (true) {
        cmd_len = strlen(command);
        gotoxy(0, screen_rows() - 1);
        printf("%-79s", command);
        gotoxy(cmd_len, screen_rows() - 1);

        getkey(&event);
        if (event.keycode == KEY_ENTER) {
            accepted = true;
            break;
        } else if (event.keycode == KEY_ESCAPE) {
            break;
        } else if (event.keycode == KEY_BACKSPACE) {
            if (cmd_len > 0)
                command[--cmd_len] = '\0';
            if (cmd_len == 0)
                break;
        } else if (event.ascii != 0) {
            if (cmd_len < 80) {
                command[cmd_len++] = key;
                command[cmd_len] = '\0';
            }
        }
    }

    draw_footer();
    return accepted;
}

// return true if handled, false if not a common key
bool handle_all_modes_key(key_event_t *event) {
    bool handled = false;

    switch (event->keycode) {
        case KEY_UP:
        case KEY_DOWN:
        case KEY_LEFT:
        case KEY_RIGHT:
        case KEY_HOME:
        case KEY_END:
        case KEY_PAGE_UP:
        case KEY_PAGE_DOWN:
        case KEY_CTRL_HOME:
        case KEY_CTRL_END:
        case KEY_CTRL_LEFT:
        case KEY_CTRL_RIGHT:
        case KEY_DELETE:
        case KEY_BACKSPACE:
            handled = true;
    }

    return false;
}


void handle_command_mode_key(key_event_t *event) {

    if ((event->ascii >= '1' && event->ascii <= '9') ||
        (count > 0 && event->ascii == '0')
    ) {
        count = (count * 10) + ('9' - event->ascii);
        return;
    }

    // vi "syntax" is "<count><op><nav>", where count is a number, operator, then a navigation key
    // usual operators are:
    // d - delete (delete text)
    // c - change (delete then enter insert mode)
    // y - yank   (yank text)
    // < and > - for shift left and right, count represents how many lines
    // 
    // for example: 
    //    3yl is copy 3 characters to right, 3yy yank 3 lines, c$ change to end of line, 
    //    dh (delete to the left), dd (delete line)


    // e.g. directional commands (apply count and operators)
    switch (event->ascii) {
        case 'h': // go left
        case 'j': // go down
        case 'k': // go up
        case 'l': // go right
        case '0': // navigate start of line
        case '$': // navigate end of line
        case 'w':
            if (operator == 'd') {
                // delete word
            } else if (operator == 0) {
                // go to next word
            }
            break;
        case 'b': // go to previous word
    }

    switch (event->ascii) {
        case 'i': // insert at cursor
            change_mode(MODE_INSERT);
            break;
        case 'I': // insert at start of line
        case 'a': // insert after cursor
        case 'A': // insert at end of line
        case 'o': // open new line below, insert
        case 'O': // insert new line here, insert
            break;
        case 'r': // replace one char, then back to command mode
            change_mode(MODE_REPLACE_ONE);
            break;
        case 'R': // permanent replace mode (clear with Esc)
            change_mode(MODE_REPLACE_CONT);
            break;
        case 'u': // undo
        case '.': // repeat last command / edit cycle
        case 'C': // clear to end of line, enter insert mode
        case 'c': // cc clear whole line, enter insert mode
            if (operator == 0) {
                operator = 'c';
            } else if (operator == 'c') {
                // clear whole line, enter insert mode
            }
            break;
        case 'x': // delete character (i.e. 'dl')
        case 'D': // delete remainder of line (i.e. 'd$')
        case 'd': // dd delete entire line
            if (operator == 0) {
                operator = 'd';
            } else if (operator == 'd') {
                // delete entire line
            }
            break;
        case 'y': // yy yank the current line
            if (operator == 0) {
                operator = 'y';
            } else if (operator == 'y') {
                // yank the current line
            }
            break;
        case 'Y': // yank current line
        case 'p': // paste after cursor
        case 'P': // paste before cursor
        case 'v': // start marking characters
        case 'V': // mark line and start marking line
        case 'n': // find next
        case 'N': // find prev
        case 'G':
            if (count == 0) {
                // go to line number <n>G
            } else if (operator == 0) {
                operator = 'G';
            } else if (operator == 'G') {
                // go to top of file
            }
            break;
        case 'Z':
            if (operator == 0) {
                operator = 'Z';
            } else if (operator == 'Z') {
                // save and exit
            }
    }
    switch (event->keycode) {
        case KEY_CTRL_G: // go to end of file
    }

    /*
        /  search forwards
        ?  search backwards
    */
}


void handle_insert_mode_character(char c) {
    bool overwrite = mode == MODE_REPLACE_ONE || mode == MODE_REPLACE_CONT;
    // insert or overwrite, depending on mode
}


void main() {
    key_event_t event;

    // supposedly load any file from args

    clear_screen();
    draw_screen();
    change_mode(MODE_COMMAND);
    while (!finished) {
        getkey(&event);

        if (handle_all_modes_key(&event))
            continue;
        
        if (mode == MODE_COMMAND || mode == MODE_VISUAL_CHARS || mode == MODE_VISUAL_LINES) {
            if (event->ascii != 0 && strchr(COMMAND_STARTING_CHARS, event->ascii) != NULL) {
                if (edit_command_line(event->ascii))
                    execute_command_line(command_line);
            } else {
                handle_command_mode_key(&event);
            }

        } else if (mode == MODE_INSERT || mode == MODE_REPLACE_ONE || mode == MODE_REPLACE_CONT) {
            if (event.keycode == KEY_ESCAPE) {
                change_mode(MODE_COMMAND);
            } else if (event->ascii != 0) {
                handle_insert_mode_character(event->ascii);
                if (mode == MODE_REPLACE_ONE)
                    change_mode(MODE_COMMAND);
            }
        }
    }

    clear_screen();
}
