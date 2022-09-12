#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <keyboard.h>
#include "line.h"
#include "buffer.h"
#include "view.h"
#include "cmd_line.h"


#define MODE_COMMAND         0
#define MODE_INSERT          1
#define MODE_REPLACE_ONE     2
#define MODE_REPLACE_CONT    3
#define MODE_VISUAL_CHARS    4
#define MODE_VISUAL_LINES    5


#define COMMAND_STARTING_CHARS   ":/?!"

int mode = 0;
bool finished = false;
int cmd_count = 1;
char operator = 0;
char cmd_line[80+1];
buffer_t *buffer;
view_t *view;




void change_mode(int new_mode) {
    char *modes[] = {
        "", // command
        "INSERT",
        "REPLACE ONE",
        "REPLACE",
        "VISUAL CHARS",
        "VISUAL LINES"
    };
    mode = new_mode;
    operator = 0;
    cmd_count = 0;
    view->ops->draw_footer(view, modes[mode]);
}

// return true if handled, false if not a common key
bool handle_all_modes_key(key_event_t *event) {
    bool handled = true;

    switch (event->keycode) {
        case KEY_UP:
            break;
        case KEY_DOWN:
            break;
        case KEY_LEFT:
            break;
        case KEY_RIGHT:
            break;
        case KEY_HOME:
            break;
        case KEY_END:
            break;
        case KEY_PAGE_UP:
            break;
        case KEY_PAGE_DOWN:
            break;
        case KEY_CTRL_HOME:
            break;
        case KEY_CTRL_END:
            break;
        case KEY_CTRL_LEFT:
            break;
        case KEY_CTRL_RIGHT:
            break;
        case KEY_DELETE:
            break;
        case KEY_BACKSPACE:
            break;
        default:
            handled = false;
            break;
    }

    return handled;
}


void handle_command_mode_key(key_event_t *event) {

    if ((event->ascii >= '1' && event->ascii <= '9') ||
        (cmd_count > 0 && event->ascii == '0')
    ) {
        cmd_count = (cmd_count * 10) + ('9' - event->ascii);
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
            break;
        case 'j': // go down
            break;
        case 'k': // go up
            break;
        case 'l': // go right
            break;
        case '0': // navigate start of line
            break;
        case '$': // navigate end of line
            break;
        case 'w':
            if (operator == 'd') {
                // delete word
            } else if (operator == 0) {
                // go to next word
            }
            break;
        case 'b': // go to previous word
            break;
    }


    switch (event->ascii) {
        case 'i': // insert at cursor
            change_mode(MODE_INSERT);
            break;
        case 'I': // insert at start of line
            break;
        case 'a': // insert after cursor
            break;
        case 'A': // insert at end of line
            break;
        case 'o': // open new line below, insert
            break;
        case 'O': // insert new line here, insert
            break;
        case 'r': // replace one char, then back to command mode
            change_mode(MODE_REPLACE_ONE);
            break;
        case 'R': // permanent replace mode (clear with Esc)
            change_mode(MODE_REPLACE_CONT);
            break;
        case 'u': // undo
            break;
        case '.': // repeat last command / edit cycle
            break;
        case 'C': // clear to end of line, enter insert mode
            break;
        case 'c': // cc clear whole line, enter insert mode
            if (operator == 0) {
                operator = 'c';
            } else if (operator == 'c') {
                // clear whole line, enter insert mode
            }
            break;
        case 'x': // delete character (i.e. 'dl')
            break;
        case 'D': // delete remainder of line (i.e. 'd$')
            break;
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
            break;
        case 'p': // paste after cursor
            break;
        case 'P': // paste before cursor
            break;
        case 'v': // start marking characters
            break;
        case 'V': // mark line and start marking line
            break;
        case 'n': // find next
            break;
        case 'N': // find prev
            break;
        case 'G':
            if (cmd_count == 0) {
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
    (void)overwrite;

    if (overwrite) {
        buffer->ops->delete_char(buffer, true);
        buffer->ops->insert_char(buffer, c);
    } else {
        buffer->ops->insert_char(buffer, c);
    }
}


void main() {
    
    clear_screen();
    printf("Welcome to vi\n");
    sleep(1000);
    printf("editing command:\n");
    if (edit_command_line(':', cmd_line)) {
        printf("Command is \"%s\"\n", cmd_line);
    } else {
        printf("Command cancelled\n");
    }
    exit(1);


    buffer = create_buffer();
    view = create_view(buffer);

    // supposedly load any file from args

    view->ops->draw_buffer(view);

    change_mode(MODE_COMMAND);
    key_event_t event;
    while (!finished) {
        getkey(&event);

        if (handle_all_modes_key(&event))
            continue;
        
        if (mode == MODE_COMMAND || mode == MODE_VISUAL_CHARS || mode == MODE_VISUAL_LINES) {
            if (event.ascii != 0 && strchr(COMMAND_STARTING_CHARS, event.ascii) != NULL) {
                if (edit_command_line(event.ascii, cmd_line))
                    execute_command_line(cmd_line, buffer, &finished);
            } else {
                handle_command_mode_key(&event);
            }

        } else if (mode == MODE_INSERT || mode == MODE_REPLACE_ONE || mode == MODE_REPLACE_CONT) {
            if (event.keycode == KEY_ESCAPE) {
                change_mode(MODE_COMMAND);
            } else if (event.ascii != 0) {
                handle_insert_mode_character(event.ascii);
                if (mode == MODE_REPLACE_ONE)
                    change_mode(MODE_COMMAND);
            }
        }
    }

    clear_screen();
}
