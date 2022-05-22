#include <stddef.h>
#include <stdbool.h>
#include "../slist.h"
#include "../string.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"


// run a modal session to read a command line from keyboard.
// keep screen updated
// return the command line enetered
// support history


static bool readline_initialized = false;
struct options_struct {
    slist_t *history;
    slist_t *keywords;
    uint8_t screen_row;
    char *prompt;
    char kill[256];
    char line[256];
    int line_pos;
    int current_history_pos;
} opts;

// initializes the readline library
void init_readline(char *prompt) {

    opts.history = slist_create();
    opts.keywords = slist_create();
    opts.prompt = prompt;
    opts.current_history_pos = -1; // past end of last entry
    opts.line_pos = 0;

    readline_initialized = true;
}

// after initializing, allows to add keywords for autocomplete
void readline_add_keyword(char *keyword) {
    slist_append(opts.keywords, keyword);
}

// after initializing, allows to add history entries for searc
void readline_add_history(char *keyword) {
    slist_append(opts.keywords, keyword);
}


// actual readline commands (we implement only what we want)
// - ctrl+a = go start of line
// - ctrl+e = go end of line
// - ctrl+f = go forwards one char
// - ctrl+b = go backwards one char
// - alt+f  = move forward a word
// - alt+b  = move backward a word
// ----------------------------------------
// - ctrl+l = clear the screen
// - ctrl+k = delete to end of line
// - ctrl+u = delete to start of line
// - ctrl+y = paste most recently killed text back in cursor position
// - alt+d  = delete word to the right
// - alt+backspace = delete word to the left
// ----------------------------------------
// - ctrl-p = previous history entry
// - ctrl-n = next history entry
// - ctrl-r = incremental search backwards in history (subsequent = find next)
// - ctrl-s = incremental search forward in history (subsequent = find next)
// ----------------------------------------
// - tab    = complete word
// - alt+?  = list possible completions (after printing them, it starts a new prompt entry)
// - alt+*  = insert all possible completions (actually alt+shift+8 as alt+8 is for arguments)
// ----------------------------------------

static void display_line();
static int find_next_word_to_the_right();
static int find_word_start_to_the_left();
static void do_accept_entry();
static void do_move_to_start_of_line();
static void do_move_to_end_of_line();
static void do_move_backwards_one_character();
static void do_move_forwards_one_character();
static void do_move_backwards_one_word();
static void do_move_forwards_one_word();
static void do_clear_screen();
static void do_delete_char_to_right();
static void do_delete_char_to_left();
static void do_delete_to_start_of_line();
static void do_delete_to_end_of_line();
static void do_delete_word_to_right();
static void do_delete_word_to_left();
static void do_paste_deleted_text();
static void do_prev_history_entry();
static void do_next_history_entry();
static void do_incremental_search_backwards();
static void do_incremental_search_forwards();
static void do_complete_word();
static void do_list_completions();
static void do_insert_completions();
static void do_handle_printable(uint8_t character);



// gets a line from user, supports history, autocomplete etc
char *readline() {
    if (!readline_initialized)
        init_readline("$");

    // it's a new entry!
    screen_get_cursor(&opts.screen_row, NULL);
    memset(opts.line, 0, sizeof(opts.line));
    opts.line_pos = 0;
    opts.current_history_pos = -1;

    while (true) {
        display_line();
        // ideally this will block us and wake us up accordingly

        struct key_event event;
        wait_keyboard_event(&event);
        if (event.special_key == KEY_ENTER) {
            do_accept_entry();
            return opts.line;
        } else if ((event.ctrl_down && event.printable == 'a') || (!event.ctrl_down && event.special_key == KEY_HOME)) {
            do_move_to_start_of_line();
        } else if ((event.ctrl_down && event.printable == 'e') || (!event.ctrl_down && event.special_key == KEY_END)) {
            do_move_to_end_of_line();
        } else if ((event.ctrl_down && event.printable == 'b') || (!event.ctrl_down && event.special_key == KEY_LEFT)) {
            do_move_backwards_one_character();
        } else if ((event.ctrl_down && event.printable == 'f') || (!event.ctrl_down && event.special_key == KEY_RIGHT)) {
            do_move_forwards_one_character();
        } else if ((event.alt_down && event.printable == 'b') || (event.ctrl_down && event.special_key == KEY_LEFT)) {
            do_move_backwards_one_word();
        } else if ((event.alt_down && event.printable == 'f') || (event.ctrl_down && event.special_key == KEY_RIGHT)) {
            do_move_forwards_one_word();
        } else if (event.ctrl_down && event.printable == 'l') {
            do_clear_screen();
        } else if ((event.ctrl_down && event.printable == 'd') || (!event.ctrl_down && event.special_key == KEY_DELETE)) {
            do_delete_char_to_right();
        } else if (!event.ctrl_down && event.special_key == KEY_BACKSPACE) {
            do_delete_char_to_left();
        } else if (event.ctrl_down && event.printable == 'u') {
            do_delete_to_start_of_line();
        } else if (event.ctrl_down && event.printable == 'k') {
            do_delete_to_end_of_line();
        } else if ((event.alt_down && event.printable == 'd') || (event.ctrl_down && event.special_key == KEY_DELETE)) {
            do_delete_word_to_right();
        } else if ((event.alt_down && event.special_key == KEY_BACKSPACE) || (event.ctrl_down && event.special_key == KEY_BACKSPACE)) {
            do_delete_word_to_left();
        } else if (event.ctrl_down && event.printable == 'y') {
            do_paste_deleted_text();
        } else if ((event.ctrl_down && event.printable == 'p') || event.special_key == KEY_UP) {
            do_prev_history_entry();
        } else if ((event.ctrl_down && event.printable == 'n') || event.special_key == KEY_DOWN) {
            do_next_history_entry();
        } else if (event.ctrl_down && event.printable == 'r') {
            do_incremental_search_backwards();
        } else if (event.ctrl_down && event.printable == 's') {
            do_incremental_search_forwards();
        } else if (event.special_key == KEY_TAB) {
            do_complete_word();
        } else if (event.alt_down && event.printable == '/') {
            do_list_completions();
        } else if (event.alt_down && event.shift_down && event.printable == '*') {
            do_insert_completions();
        } else if (event.printable != 0) {
            do_handle_printable(event.printable);
        }
    }
}



// static void old_get_command(char *prompt) {
//     // honor backspace, Ctrl+U (delete all), Ctrl+L (clear),
//     // maybe do history with arrow keys.
//     // maybe do auto completion with tab.
//     struct key_event event;

//     // collect key presseses, until enter is pressed
//     memset(command, 0, sizeof(command));
//     command_len = 0;
//     printf("%s", prompt);
//     while (true) {
//         wait_keyboard_event(&event);
//         if (event.special_key == KEY_ENTER) {
//             strcpy(prev_command, command);
//             printf("\r\n");
//             return;
//         } else if (event.special_key == KEY_UP && strlen(prev_command) > 0) {
//             strcpy(command, prev_command);
//             command_len = strlen(command);
//             printf("\r%79s\r%s%s", " ", prompt, command);
//         } else if (event.special_key == KEY_ESCAPE) {
//             command_len = 0;
//             command[command_len] = '\0';
//             printf(" (esc)\n%s", prompt);
//         } else if (event.special_key == KEY_BACKSPACE) {
//             if (command_len > 0) {
//                 command[--command_len] = '\0';
//                 printf("\b \b");
//             }
//         } else if (event.ctrl_down) {
//             switch (event.printable) {
//                 case 'l':
//                     screen_clear() {

// }


//                     printf("%s%s", prompt, command);
//                     break;
//                 case 'u':
//                     command_len = 0;
//                     command[command_len] = '\0';
//                     printf("\r%79s\r%s%s", " ", prompt, command);
//                     break;
//             }
//         } else if (event.printable) {
//             command[command_len++] = event.printable;
//             command[command_len] = '\0';
//             printf("%c", event.printable);
//         }
//     }
// }

static void display_line() {
    screen_set_cursor(opts.screen_row, 0);
    screen_write(opts.prompt);
    screen_write(opts.line);
    int remaining = 79 - strlen(opts.prompt) - strlen(opts.line);
    while (remaining-- > 0)
        screen_putchar(' ');
    screen_set_cursor(opts.screen_row, strlen(opts.prompt) + opts.line_pos);

}

static int find_next_word_to_the_right() {
    int target = opts.line_pos;

    // skip current word
    while (opts.line[target] != '\0' && opts.line[target] != ' ')
        target++;
    if (opts.line[target] == '\0')
        return target;

    // skip whitespace
    while (opts.line[target] != '\0' && opts.line[target] == ' ')
        target++;
    
    // we should be either at next word, or at line's end
    return target;
}

static int find_word_start_to_the_left() {
    int target = opts.line_pos;

    // skip whitespace
    while (target > 0 && opts.line[target - 1] == ' ')
        target--;
    if (target == 0)
        return target;

    // find start of current word
    while (target > 0 && opts.line[target - 1] != ' ')
        target--;
    
    // we should be either at prev word, or at dead start
    return target;
}

static void do_accept_entry() {
    // if we are searching, bring from history
    
    printf("\n");
    if (strlen(opts.line) == 0)
        return;
    
    slist_append(opts.history, opts.line);
}

static void do_move_to_start_of_line() {
    opts.line_pos = 0;
}

static void do_move_to_end_of_line() {
    opts.line_pos = strlen(opts.line);
}

static void do_move_backwards_one_character() {
    if (opts.line_pos > 0)
        opts.line_pos--;
}

static void do_move_forwards_one_character() {
    if (opts.line_pos < strlen(opts.line))
        opts.line_pos++;
}

static void do_move_backwards_one_word() {
    opts.line_pos = find_word_start_to_the_left();
}

static void do_move_forwards_one_word() {
    opts.line_pos = find_next_word_to_the_right();
}

static void do_clear_screen() {
    screen_clear();
    opts.screen_row = 0;
}

static void do_delete_char_to_right() {
    if (opts.line_pos < strlen(opts.line))
        memmove(opts.line + opts.line_pos, opts.line + opts.line_pos + 1, strlen(opts.line) - opts.line_pos);
}

static void do_delete_char_to_left() {
    if (opts.line_pos > 0) {
        opts.line_pos--;
        memmove(opts.line + opts.line_pos, opts.line + opts.line_pos + 1, strlen(opts.line) - opts.line_pos);
    }
}

static void do_delete_word_to_right() {
    int target = find_next_word_to_the_right();
    if (target == opts.line_pos)
        return;

    memcpy(opts.kill, opts.line + opts.line_pos, target - opts.line_pos);
    opts.kill[target - opts.line_pos] = '\0';
    memmove(opts.line + opts.line_pos, opts.line + target, strlen(opts.line) - target + 1);
}

static void do_delete_word_to_left() {
    int target = find_word_start_to_the_left();
    if (target == opts.line_pos)
        return;

    memcpy(opts.kill, opts.line + target, opts.line_pos - target);
    opts.kill[opts.line_pos - target] = '\0';
    memmove(opts.line + target, opts.line + opts.line_pos, strlen(opts.line) - opts.line_pos + 1);
    opts.line_pos = target;
}

static void do_delete_to_start_of_line() {
    memcpy(opts.kill, opts.line, opts.line_pos);
    opts.kill[opts.line_pos] = '\0';
    memmove(opts.line, opts.line + opts.line_pos, strlen(opts.line) + 1 - opts.line_pos);
    opts.line_pos = 0;
}

static void do_delete_to_end_of_line() {
    memcpy(opts.kill, opts.line + opts.line_pos, strlen(opts.line) - opts.line_pos + 1);
    opts.line[opts.line_pos] = 0;
}

static void do_paste_deleted_text() {
    if (strlen(opts.kill) == 0)
        return;
    
    if (opts.line_pos == strlen(opts.line)) {
        // just append, including zero terminator
        strcpy(opts.line + opts.line_pos, opts.kill);
        opts.line_pos += strlen(opts.kill);
    } else {
        // make some room
        memmove(
            opts.line + opts.line_pos + strlen(opts.kill), 
            opts.line + opts.line_pos,
            strlen(opts.line) - opts.line_pos + 1);
        memcpy(opts.line + opts.line_pos, opts.kill, strlen(opts.kill));
        opts.line_pos += strlen(opts.kill);
    }
}

static void do_prev_history_entry() {
}

static void do_next_history_entry() {
}

static void do_incremental_search_backwards() {
}

static void do_incremental_search_forwards() {
}

static void do_complete_word() {
    // if only one completion exists, do it,
    // otherwise, print them (same as list completions)
}

static void do_list_completions() {
    // print a new line, all possible completions, then a new prompt, our buffer, move to current point
}

static void do_insert_completions() {
    // insert all possible completions at current point
}

static void do_handle_printable(uint8_t character) {
    // if we are searching, add but also search, otherwise, insert/append at position



    if (strlen(opts.line) == 0 && opts.line_pos == 0) {
        opts.line[opts.line_pos] = character;
        opts.line_pos++;
        opts.line[opts.line_pos] = '\0';
    } else if (opts.line_pos == strlen(opts.line) && opts.line_pos < (int)sizeof(opts.line) - 1) {
        opts.line[opts.line_pos] = character;
        opts.line_pos++;
        opts.line[opts.line_pos] = '\0';
    } else if (strlen(opts.line) == sizeof(opts.line) - 1) {
        // we could beep
        return;
    } else {
        // make some room
        memmove(
            opts.line + opts.line_pos + 1, 
            opts.line + opts.line_pos,
            strlen(opts.line) - opts.line_pos + 1);
        opts.line[opts.line_pos] = character;
        opts.line_pos++;
    }
}



