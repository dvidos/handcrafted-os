#include <stddef.h>
#include <stdbool.h>
#include "../klib/slist.h"
#include "../string.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"
#include "../klog.h"


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
    char line[128];
    int line_pos;
    int history_pos;
    char kill[128];
    bool searching;
    char search_term[64];
    char word[64];
    bool searching_backwards;
    bool search_failed;
    bool completion_list_pending;
} opts;

// initializes the readline library
void init_readline(char *prompt) {

    opts.history = slist_create();
    opts.keywords = slist_create();
    opts.prompt = prompt;
    opts.history_pos = -1; // past end of last entry
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
// - ctrl+? = help
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
static void display_help();
static void display_history();
static int find_next_word_to_the_right();
static int find_word_start_to_the_left();
static void insert_in_line(char *str);
static void delete_from_line(int len, bool from_the_right, bool save_to_kill);
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
static void do_yank_deleted_text();
static void do_prev_history_entry();
static void do_next_history_entry();
static void do_incremental_search(bool backwards);
static void do_complete_word();
static void do_list_completions();
static void do_insert_completions();
static void do_handle_printable(uint8_t character);
static void perform_incremental_search();


// gets a line from user, supports history, autocomplete etc
char *readline() {
    if (!readline_initialized)
        init_readline("$");

    // it's a new entry!
    screen_get_cursor(&opts.screen_row, NULL);
    memset(opts.line, 0, sizeof(opts.line));
    opts.line_pos = 0;
    opts.history_pos = -1;

    while (true) {
        display_line();
        // ideally this will block us and wake us up accordingly
        key_event_t e;
        wait_keyboard_event(&e);
        if (e.special_key == KEY_ENTER) {
            do_accept_entry();
            return opts.line;
        } else if (e.ctrl_down && e.printable == '/') {
            display_help();
        } else if ((e.ctrl_down && e.printable == 'a') || (!e.ctrl_down && e.special_key == KEY_HOME)) {
            do_move_to_start_of_line();
        } else if ((e.ctrl_down && e.printable == 'e') || (!e.ctrl_down && e.special_key == KEY_END)) {
            do_move_to_end_of_line();
        } else if ((e.ctrl_down && e.printable == 'b') || (!e.ctrl_down && e.special_key == KEY_LEFT)) {
            do_move_backwards_one_character();
        } else if ((e.ctrl_down && e.printable == 'f') || (!e.ctrl_down && e.special_key == KEY_RIGHT)) {
            do_move_forwards_one_character();
        } else if ((e.alt_down && e.printable == 'b') || (e.ctrl_down && e.special_key == KEY_LEFT)) {
            do_move_backwards_one_word();
        } else if ((e.alt_down && e.printable == 'f') || (e.ctrl_down && e.special_key == KEY_RIGHT)) {
            do_move_forwards_one_word();
        } else if (e.ctrl_down && e.printable == 'l') {
            do_clear_screen();
        } else if ((e.ctrl_down && e.printable == 'd') || (!e.ctrl_down && e.special_key == KEY_DELETE)) {
            do_delete_char_to_right();
        } else if (!e.ctrl_down && e.special_key == KEY_BACKSPACE) {
            do_delete_char_to_left();
        } else if (e.ctrl_down && e.printable == 'u') {
            do_delete_to_start_of_line();
        } else if (e.ctrl_down && e.printable == 'k') {
            do_delete_to_end_of_line();
        } else if ((e.alt_down && e.printable == 'd') || (e.ctrl_down && e.special_key == KEY_DELETE)) {
            do_delete_word_to_right();
        } else if ((e.alt_down && e.special_key == KEY_BACKSPACE) || (e.ctrl_down && e.special_key == KEY_BACKSPACE)) {
            do_delete_word_to_left();
        } else if (e.ctrl_down && e.printable == 'y') {
            do_yank_deleted_text();
        } else if ((e.ctrl_down && e.printable == 'p') || e.special_key == KEY_UP) {
            do_prev_history_entry();
        } else if ((e.ctrl_down && e.printable == 'n') || e.special_key == KEY_DOWN) {
            do_next_history_entry();
        } else if (e.ctrl_down && e.printable == 'h') {
            display_history();
        } else if (e.ctrl_down && e.printable == 'r') {
            do_incremental_search(true);
        } else if (e.ctrl_down && e.printable == 's') {
            do_incremental_search(false);
        } else if (e.special_key == KEY_TAB) {
            do_complete_word();
        } else if (e.alt_down && e.printable == '/') {
            do_list_completions();
        } else if (e.alt_down && e.shift_down && e.printable == '*') {
            do_insert_completions();
        } else if (e.printable != 0) {
            do_handle_printable(e.printable);
        }
    }
}



// static void old_get_command(char *prompt) {
//     // honor backspace, Ctrl+U (delete all), Ctrl+L (clear),
//     // maybe do history with arrow keys.
//     // maybe do auto completion with tab.
//     key_event_t e;

//     // collect key presseses, until enter is pressed
//     memset(command, 0, sizeof(command));
//     command_len = 0;
//     printf("%s", prompt);
//     while (true) {
//         wait_keyboard_event(&e);
//         if (e.special_key == KEY_ENTER) {
//             strcpy(prev_command, command);
//             printf("\r\n");
//             return;
//         } else if (e.special_key == KEY_UP && strlen(prev_command) > 0) {
//             strcpy(command, prev_command);
//             command_len = strlen(command);
//             printf("\r%79s\r%s%s", " ", prompt, command);
//         } else if (e.special_key == KEY_ESCAPE) {
//             command_len = 0;
//             command[command_len] = '\0';
//             printf(" (esc)\n%s", prompt);
//         } else if (e.special_key == KEY_BACKSPACE) {
//             if (command_len > 0) {
//                 command[--command_len] = '\0';
//                 printf("\b \b");
//             }
//         } else if (e.ctrl_down) {
//             switch (e.printable) {
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
//         } else if (e.printable) {
//             command[command_len++] = e.printable;
//             command[command_len] = '\0';
//             printf("%c", e.printable);
//         }
//     }
// }

static void display_line() {
    screen_set_cursor(opts.screen_row, 0);
    if (opts.searching) {
        screen_write("(");
        if (opts.search_failed)
            screen_write("failed ");
        screen_write(opts.searching_backwards ? "reverse-i-search)`" : "forward-i-search)`");
        screen_write(opts.search_term);
        screen_write("': ");
        screen_write(opts.line);
        int prompt_len = 1 + (opts.search_failed ? 7 : 0) + 18 + strlen(opts.search_term) + 3;
        int remaining = 79 - prompt_len - strlen(opts.line);
        while (remaining-- > 0)
            screen_putchar(' ');
        screen_set_cursor(opts.screen_row, prompt_len);
    } else {
        screen_write(opts.prompt);
        screen_write(opts.line);
        int remaining = 79 - strlen(opts.prompt) - strlen(opts.line);
        while (remaining-- > 0)
            screen_putchar(' ');
        screen_set_cursor(opts.screen_row, strlen(opts.prompt) + opts.line_pos);
    }
}

static void display_help() {
    screen_write("\n");
    screen_write("--- Movement commands ---\n");
    screen_write(" ctrl-a      go start of line\n");
    screen_write(" ctrl-e      go end of line\n");
    screen_write(" ctrl-f      go forwards one char\n");
    screen_write(" ctrl-b      go backwards one char\n");
    screen_write(" alt-f       move forward a word\n");
    screen_write(" alt-b       move backward a word\n");
    screen_write("--- Editing commands ---\n");
    screen_write(" ctrl-k      delete to end of line\n");
    screen_write(" ctrl-u      delete to start of line\n");
    screen_write(" ctrl-y      yank most recently killed text in cursor position\n");
    screen_write(" alt-d       delete word to the right\n");
    screen_write(" alt-backsp  delete word to the left\n");
    screen_write(" ctrl-l      clear the screen\n");
    screen_write("--- History commands ---\n");
    screen_write(" ctrl-p      previous history entry\n");
    screen_write(" ctrl-n      next history entry\n");
    screen_write(" ctrl-r      incremental search backwards in history\n");
    screen_write(" ctrl-s      incremental search forward in history\n");
    screen_write("--- Completion commands ---\n");
    screen_write(" tab         complete word\n");
    screen_write(" alt-?       list possible completions\n");
    screen_write(" alt-*       insert all possible completions\n");

    screen_get_cursor(&opts.screen_row, NULL);
}

static void display_history() {
    printf("\n");
    int len = slist_size(opts.history);
    for (int i = 0; i < len; i++)
        printf(" %d: %s\n", i, slist_get(opts.history, i));

    screen_get_cursor(&opts.screen_row, NULL);
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

static void insert_in_line(char *str) {
    if (opts.line_pos == strlen(opts.line)) {
        // just append, including zero terminator
        strcpy(opts.line + opts.line_pos, str);
        opts.line_pos += strlen(str);
    } else {
        // make some room, then insert
        memmove(
            opts.line + opts.line_pos + strlen(str), 
            opts.line + opts.line_pos,
            strlen(opts.line) - opts.line_pos + 1);
        memcpy(opts.line + opts.line_pos, str, strlen(str));
        opts.line_pos += strlen(str);
    }
}

static void delete_from_line(int len, bool from_the_right, bool save_to_kill) {
    if (from_the_right) {
        if (opts.line_pos == strlen(opts.line))
            return;
        if (save_to_kill) {
            memcpy(opts.kill, opts.line + opts.line_pos, len);
            opts.kill[len] = '\0';
        }
        memmove(opts.line + opts.line_pos, opts.line + opts.line_pos + len, strlen(opts.line) - opts.line_pos - len + 1);
    } else {
        if (opts.line_pos == 0)
            return;
        if (save_to_kill) {
            memcpy(opts.kill, opts.line + opts.line_pos - len, len);
            opts.kill[len] = '\0';
        }
        memmove(opts.line + opts.line_pos - len, opts.line + opts.line_pos, strlen(opts.line) - opts.line_pos + 1);
        opts.line_pos -= len;
   }
}

static void find_word_to_cursor() {
    int word_start = find_word_start_to_the_left();
    int word_len = opts.line_pos - word_start <= (int)sizeof(opts.word) - 1 
        ? opts.line_pos - word_start
        : (int)sizeof(opts.word) - 1; 
    memcpy(opts.word, opts.line + word_start, word_len);
    opts.word[word_len] = '\0';
}

static void do_accept_entry() {
    opts.searching = false;    

    printf("\n");
    if (strlen(opts.line) == 0)
        return;
    
    slist_append(opts.history, opts.line);
}

static void do_move_to_start_of_line() {
    opts.searching = false;
    opts.line_pos = 0;
}

static void do_move_to_end_of_line() {
    opts.searching = false;
    opts.line_pos = strlen(opts.line);
}

static void do_move_backwards_one_character() {
    opts.searching = false;
    if (opts.line_pos > 0)
        opts.line_pos--;
}

static void do_move_forwards_one_character() {
    opts.searching = false;
    if (opts.line_pos < strlen(opts.line))
        opts.line_pos++;
}

static void do_move_backwards_one_word() {
    opts.searching = false;
    opts.line_pos = find_word_start_to_the_left();
}

static void do_move_forwards_one_word() {
    opts.searching = false;
    opts.line_pos = find_next_word_to_the_right();
}

static void do_clear_screen() {
    screen_clear();
    opts.searching = false;
    opts.screen_row = 0;
}

static void do_delete_char_to_right() {
    opts.searching = false;
    delete_from_line(1, true, false);
}

static void do_delete_char_to_left() {
    if (opts.searching) {
        if (strlen(opts.search_term) > 0) {
            opts.search_term[strlen(opts.search_term) - 1] = '\0';
            perform_incremental_search();
        }
    } else {
        delete_from_line(1, false, false);
    }
}

static void do_delete_word_to_right() {
    opts.searching = false;

    int target = find_next_word_to_the_right();
    if (target == opts.line_pos)
        return;

    delete_from_line(target - opts.line_pos, true, true);
}

static void do_delete_word_to_left() {
    opts.searching = false;

    int target = find_word_start_to_the_left();
    if (target == opts.line_pos)
        return;

    delete_from_line(opts.line_pos - target, false, true);
}

static void do_delete_to_start_of_line() {
    opts.searching = false;
    delete_from_line(opts.line_pos, false, true);
}

static void do_delete_to_end_of_line() {
    opts.searching = false;
    delete_from_line(strlen(opts.line) - opts.line_pos, true, true);
}

static void do_yank_deleted_text() {
    opts.searching = false;
    insert_in_line(opts.kill);
}

static void do_prev_history_entry() {
    opts.searching = false;
    if (opts.history_pos == -1)
        opts.history_pos = slist_size(opts.history);
    
    if (opts.history_pos == 0)
        return;

    opts.history_pos--;
    strcpy(opts.line, slist_get(opts.history, opts.history_pos));
    opts.line_pos = strlen(opts.line);
}

static void do_next_history_entry() {
    opts.searching = false;
    if (opts.history_pos == -1)
        opts.history_pos = slist_size(opts.history);
    
    if (opts.history_pos == slist_size(opts.history))
        return;
    
    // moving past the last history items, brings a new empty line
    opts.history_pos++;
    if (opts.history_pos == slist_size(opts.history)) {
        opts.line[0] = '\0';
    } else if (opts.history_pos < slist_size(opts.history)) {
        strcpy(opts.line, slist_get(opts.history, opts.history_pos));
    }
    opts.line_pos = strlen(opts.line);
}

static void do_incremental_search(bool backwards) {
    if (!opts.searching) {
        opts.searching = true;
        opts.searching_backwards = backwards;
        opts.search_term[0] = '\0';
        opts.search_failed = false;
    } else {
        perform_incremental_search();
    }
}

static void do_complete_word() {
    find_word_to_cursor();
    if (strlen(opts.word) == 0)
        return;

    int count = slist_count_prefix(opts.keywords, opts.word);
    if (count == 1) {
        int index = slist_indexof_prefix(opts.keywords, opts.word, 0);
        char *completed = slist_get(opts.keywords, index);
        insert_in_line(completed + strlen(opts.word));
        opts.completion_list_pending = false;
    } else if (count > 1) {
        if (opts.completion_list_pending) {
            do_list_completions();
        } else {
            opts.completion_list_pending = true;
        }
    }
}

static void do_list_completions() {
    find_word_to_cursor();
    if (strlen(opts.word) == 0)
        return;

    printf("\n");
    int start = 0;
    int pos = slist_indexof_prefix(opts.keywords, opts.word, start);
    while (pos != -1) {
        printf("> %s\n", slist_get(opts.keywords, pos));
        start = pos + 1;
        pos = slist_indexof_prefix(opts.keywords, opts.word, start);
    }

    screen_get_cursor(&opts.screen_row, NULL);
}

static void do_insert_completions() {
    find_word_to_cursor();
    if (strlen(opts.word) == 0)
        return;

    delete_from_line(strlen(opts.word), false, false);
    int start = 0;
    int pos = slist_indexof_prefix(opts.keywords, opts.word, start);
    while (pos != -1) {
        insert_in_line(slist_get(opts.keywords, pos));
        insert_in_line(" ");
        start = pos + 1;
        pos = slist_indexof_prefix(opts.keywords, opts.word, start);
    }

    screen_get_cursor(&opts.screen_row, NULL);
}

static void do_handle_printable(uint8_t character) {
    if (opts.searching) {
        // if we are searching, add but also search, otherwise, insert/append at position
        int len = strlen(opts.search_term);
        if (strlen(opts.search_term) >= (int)sizeof(opts.search_term) - 2)
            return;
        
        opts.search_term[len] = character;
        opts.search_term[len + 1] = '\0';
        opts.history_pos = -1;
        perform_incremental_search();
    } else {
        // not searching, treat printable as letter
        char buffer[2];
        buffer[0] = character;
        buffer[1] = '\0';
        insert_in_line(buffer);
    }
}

static void perform_incremental_search() {
    if (!opts.searching || strlen(opts.search_term) == 0)
        return;
    
    int start, pos;
    if (opts.searching_backwards) {
        start = (opts.history_pos == -1) ? slist_size(opts.history) - 1 : opts.history_pos - 1;
        pos = slist_last_indexof_containing(opts.history, opts.search_term, start);
    } else {
        start = (opts.history_pos == -1) ? 0 : opts.history_pos + 1;
        pos = slist_indexof_containing(opts.history, opts.search_term, start);
    }
    opts.search_failed = (pos == -1);
    if (!opts.search_failed) {
        opts.history_pos = pos;
        strcpy(opts.line, slist_get(opts.history, pos));
    }
}

