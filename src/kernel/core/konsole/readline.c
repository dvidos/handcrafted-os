#include <stddef.h>
#include <stdbool.h>
#include "../klib/slist.h"
#include "../string.h"
#include "../klog.h"
#include "../devices/tty.h"
#include "../memory/kheap.h"


// run a modal session to read a command line from keyboard, keep screen updated
// return the command line entered, support history, search, auto complete


struct readline_info {
    slist_t *history;
    slist_t *keywords;
    uint8_t cursor_row;
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
};

typedef struct readline_info readline_t;


// initializes the readline library
readline_t *init_readline(char *prompt) {
    readline_t *rl = kmalloc(sizeof(readline_t));
    memset(rl, 0, sizeof(readline_t));

    rl->history = slist_create();
    rl->keywords = slist_create();
    rl->prompt = prompt;
    rl->history_pos = -1; // past end of last entry
    rl->line_pos = 0;

    return rl;
}

// after initializing, allows to add keywords for autocomplete
void readline_add_keyword(readline_t *rl, char *keyword) {
    slist_append(rl->keywords, keyword);
}

// after initializing, allows to add history entries for searc
void readline_add_history(readline_t *rl, char *keyword) {
    slist_append(rl->keywords, keyword);
}


static void display_line(readline_t *rl);
static void display_help(readline_t *rl);
static void display_history(readline_t *rl);
static int find_next_word_to_the_right(readline_t *rl);
static int find_word_start_to_the_left(readline_t *rl);
static void insert_in_line(readline_t *rl, char *str);
static void delete_from_line(readline_t *rl, int len, bool from_the_right, bool save_to_kill);
static void do_accept_entry(readline_t *rl);
static void do_move_to_start_of_line(readline_t *rl);
static void do_move_to_end_of_line(readline_t *rl);
static void do_move_backwards_one_character(readline_t *rl);
static void do_move_forwards_one_character(readline_t *rl);
static void do_move_backwards_one_word(readline_t *rl);
static void do_move_forwards_one_word(readline_t *rl);
static void do_clear_screen(readline_t *rl);
static void do_delete_char_to_right(readline_t *rl);
static void do_delete_char_to_left(readline_t *rl);
static void do_delete_to_start_of_line(readline_t *rl);
static void do_delete_to_end_of_line(readline_t *rl);
static void do_delete_word_to_right(readline_t *rl);
static void do_delete_word_to_left(readline_t *rl);
static void do_yank_deleted_text(readline_t *rl);
static void do_prev_history_entry(readline_t *rl);
static void do_next_history_entry(readline_t *rl);
static void do_incremental_search(readline_t *rl, bool backwards);
static void do_complete_word(readline_t *rl);
static void do_list_completions(readline_t *rl);
static void do_insert_completions(readline_t *rl);
static void do_handle_printable(readline_t *rl, uint8_t character);
static void perform_incremental_search(readline_t *rl);


// gets a line from user, supports history, autocomplete etc
char *readline(readline_t *rl) {
    // it's a new entry!
    tty_get_cursor(&rl->cursor_row, NULL);
    memset(rl->line, 0, sizeof(rl->line));
    rl->line_pos = 0;
    rl->history_pos = -1;

    while (true) {
        display_line(rl);
        // ideally this will block us and wake us up accordingly
        key_event_t e;
        tty_read_key(&e);
        if (e.special_key == KEY_ENTER) {
            do_accept_entry(rl);
            return rl->line;
        } else if (e.ctrl_down && e.printable == '/') {
            display_help(rl);
        } else if ((e.ctrl_down && e.printable == 'a') || (!e.ctrl_down && e.special_key == KEY_HOME)) {
            do_move_to_start_of_line(rl);
        } else if ((e.ctrl_down && e.printable == 'e') || (!e.ctrl_down && e.special_key == KEY_END)) {
            do_move_to_end_of_line(rl);
        } else if ((e.ctrl_down && e.printable == 'b') || (!e.ctrl_down && e.special_key == KEY_LEFT)) {
            do_move_backwards_one_character(rl);
        } else if ((e.ctrl_down && e.printable == 'f') || (!e.ctrl_down && e.special_key == KEY_RIGHT)) {
            do_move_forwards_one_character(rl);
        } else if ((e.alt_down && e.printable == 'b') || (e.ctrl_down && e.special_key == KEY_LEFT)) {
            do_move_backwards_one_word(rl);
        } else if ((e.alt_down && e.printable == 'f') || (e.ctrl_down && e.special_key == KEY_RIGHT)) {
            do_move_forwards_one_word(rl);
        } else if (e.ctrl_down && e.printable == 'l') {
            do_clear_screen(rl);
        } else if ((e.ctrl_down && e.printable == 'd') || (!e.ctrl_down && e.special_key == KEY_DELETE)) {
            do_delete_char_to_right(rl);
        } else if (!e.ctrl_down && e.special_key == KEY_BACKSPACE) {
            do_delete_char_to_left(rl);
        } else if (e.ctrl_down && e.printable == 'u') {
            do_delete_to_start_of_line(rl);
        } else if (e.ctrl_down && e.printable == 'k') {
            do_delete_to_end_of_line(rl);
        } else if ((e.alt_down && e.printable == 'd') || (e.ctrl_down && e.special_key == KEY_DELETE)) {
            do_delete_word_to_right(rl);
        } else if ((e.alt_down && e.special_key == KEY_BACKSPACE) || (e.ctrl_down && e.special_key == KEY_BACKSPACE)) {
            do_delete_word_to_left(rl);
        } else if (e.ctrl_down && e.printable == 'y') {
            do_yank_deleted_text(rl);
        } else if ((e.ctrl_down && e.printable == 'p') || e.special_key == KEY_UP) {
            do_prev_history_entry(rl);
        } else if ((e.ctrl_down && e.printable == 'n') || e.special_key == KEY_DOWN) {
            do_next_history_entry(rl);
        } else if (e.ctrl_down && e.printable == 'h') {
            display_history(rl);
        } else if (e.ctrl_down && e.printable == 'r') {
            do_incremental_search(rl, true);
        } else if (e.ctrl_down && e.printable == 's') {
            do_incremental_search(rl, false);
        } else if (e.special_key == KEY_TAB) {
            do_complete_word(rl);
        } else if (e.alt_down && e.printable == '/') {
            do_list_completions(rl);
        } else if (e.alt_down && e.shift_down && e.printable == '*') {
            do_insert_completions(rl);
        } else if (e.printable != 0) {
            do_handle_printable(rl, e.printable);
        }
    }
}


static void display_line(readline_t *rl) {
    char buff[80];

    tty_set_cursor(rl->cursor_row, 0);
    if (rl->searching) {
        sprintfn(buff, sizeof(buff),
            "(%s%s-i-search)`%s': ",
            rl->search_failed ? "failed " : "",
            rl->searching_backwards ? "reverse" : "forward",
            rl->search_term
        );
        tty_write(buff);
        tty_write(rl->line);
        int prompt_len = 1 + (rl->search_failed ? 7 : 0) + 18 + strlen(rl->search_term) + 3;
        int remaining = 79 - prompt_len - strlen(rl->line);
        memset(buff, ' ', sizeof(buff));
        buff[remaining] = '\0';
        tty_write(buff);
        tty_set_cursor(rl->cursor_row, prompt_len);
    } else {
        tty_write(rl->prompt);
        tty_write(rl->line);
        int remaining = 79 - strlen(rl->prompt) - strlen(rl->line);
        memset(buff, ' ', sizeof(buff));
        buff[remaining] = '\0';
        tty_write(buff);
        tty_set_cursor(rl->cursor_row, strlen(rl->prompt) + rl->line_pos);
    }
}

static void display_help(readline_t *rl) {
    //         0--------1---------2---------3---------4---------5---------6---------7---------8
    //         12345678901234567890123456789012345678901234567890123456789012345678901234567890
    tty_write("\n");
    tty_write("------ Movement commands ------        ------ Editing commands ------\n");
    tty_write("ctrl-a   go start of line              ctrl-k    delete to end of line\n");
    tty_write("ctrl-e   go end of line                ctrl-u    delete to start of line\n");
    tty_write("ctrl-f   go forwards one char          ctrl-y    yank recently deleted text\n");
    tty_write("ctrl-b   go backwards one char         alt-d     delete word to the right\n");
    tty_write("alt-f    forward a word                alt-bksp  delete word to the left\n");
    tty_write("alt-b    backward a word               ctrl-l    clear the screen\n");
    tty_write(" \n");
    tty_write("------ History commands ------         ------ Completion commands ------\n");
    tty_write("ctrl-p   previous history entry        tab       complete word\n");
    tty_write("ctrl-n   next history entry            alt-?     list possible completions\n");
    tty_write("ctrl-r   incremental backwards search  alt-*     insert possible completions\n");
    tty_write("ctrl-s   incremental forward search    \n");
    tty_write("ctrl-h   display history entries       \n");
    tty_write(" \n");

    tty_get_cursor(&rl->cursor_row, NULL);
}

static void display_history(readline_t *rl) {
    char buff[80];
    tty_write("\n");
    int len = slist_size(rl->history);
    for (int i = 0; i < len; i++) {
        sprintfn(buff, sizeof(buff), " %d: %s\n", i, slist_get(rl->history, i));
        tty_write(buff);
    }

    tty_get_cursor(&rl->cursor_row, NULL);
}

static int find_next_word_to_the_right(readline_t *rl) {
    int target = rl->line_pos;

    // skip current word
    while (rl->line[target] != '\0' && rl->line[target] != ' ')
        target++;
    if (rl->line[target] == '\0')
        return target;

    // skip whitespace
    while (rl->line[target] != '\0' && rl->line[target] == ' ')
        target++;
    
    // we should be either at next word, or at line's end
    return target;
}

static int find_word_start_to_the_left(readline_t *rl) {
    int target = rl->line_pos;

    // skip whitespace
    while (target > 0 && rl->line[target - 1] == ' ')
        target--;
    if (target == 0)
        return target;

    // find start of current word
    while (target > 0 && rl->line[target - 1] != ' ')
        target--;
    
    // we should be either at prev word, or at dead start
    return target;
}

static void insert_in_line(readline_t *rl, char *str) {
    if (rl->line_pos == strlen(rl->line)) {
        // just append, including zero terminator
        strcpy(rl->line + rl->line_pos, str);
        rl->line_pos += strlen(str);
    } else {
        // make some room, then insert
        memmove(
            rl->line + rl->line_pos + strlen(str), 
            rl->line + rl->line_pos,
            strlen(rl->line) - rl->line_pos + 1);
        memcpy(rl->line + rl->line_pos, str, strlen(str));
        rl->line_pos += strlen(str);
    }
}

static void delete_from_line(readline_t *rl, int len, bool from_the_right, bool save_to_kill) {
    if (from_the_right) {
        if (rl->line_pos == strlen(rl->line))
            return;
        if (save_to_kill) {
            memcpy(rl->kill, rl->line + rl->line_pos, len);
            rl->kill[len] = '\0';
        }
        memmove(rl->line + rl->line_pos, rl->line + rl->line_pos + len, strlen(rl->line) - rl->line_pos - len + 1);
    } else {
        if (rl->line_pos == 0)
            return;
        if (save_to_kill) {
            memcpy(rl->kill, rl->line + rl->line_pos - len, len);
            rl->kill[len] = '\0';
        }
        memmove(rl->line + rl->line_pos - len, rl->line + rl->line_pos, strlen(rl->line) - rl->line_pos + 1);
        rl->line_pos -= len;
   }
}

static void find_word_to_cursor(readline_t *rl) {
    int word_start = find_word_start_to_the_left(rl);
    int word_len = rl->line_pos - word_start <= (int)sizeof(rl->word) - 1 
        ? rl->line_pos - word_start
        : (int)sizeof(rl->word) - 1; 
    memcpy(rl->word, rl->line + word_start, word_len);
    rl->word[word_len] = '\0';
}

static void do_accept_entry(readline_t *rl) {
    rl->searching = false;    

    tty_write("\n");
    if (strlen(rl->line) == 0)
        return;
    
    slist_append(rl->history, rl->line);
}

static void do_move_to_start_of_line(readline_t *rl) {
    rl->searching = false;
    rl->line_pos = 0;
}

static void do_move_to_end_of_line(readline_t *rl) {
    rl->searching = false;
    rl->line_pos = strlen(rl->line);
}

static void do_move_backwards_one_character(readline_t *rl) {
    rl->searching = false;
    if (rl->line_pos > 0)
        rl->line_pos--;
}

static void do_move_forwards_one_character(readline_t *rl) {
    rl->searching = false;
    if (rl->line_pos < strlen(rl->line))
        rl->line_pos++;
}

static void do_move_backwards_one_word(readline_t *rl) {
    rl->searching = false;
    rl->line_pos = find_word_start_to_the_left(rl);
}

static void do_move_forwards_one_word(readline_t *rl) {
    rl->searching = false;
    rl->line_pos = find_next_word_to_the_right(rl);
}

static void do_clear_screen(readline_t *rl) {
    tty_clear();
    tty_get_cursor(&rl->cursor_row, NULL);
    rl->searching = false;
}

static void do_delete_char_to_right(readline_t *rl) {
    rl->searching = false;
    delete_from_line(rl, 1, true, false);
}

static void do_delete_char_to_left(readline_t *rl) {
    if (rl->searching) {
        if (strlen(rl->search_term) > 0) {
            rl->search_term[strlen(rl->search_term) - 1] = '\0';
            perform_incremental_search(rl);
        }
    } else {
        delete_from_line(rl, 1, false, false);
    }
}

static void do_delete_word_to_right(readline_t *rl) {
    rl->searching = false;

    int target = find_next_word_to_the_right(rl);
    if (target == rl->line_pos)
        return;

    delete_from_line(rl, target - rl->line_pos, true, true);
}

static void do_delete_word_to_left(readline_t *rl) {
    rl->searching = false;

    int target = find_word_start_to_the_left(rl);
    if (target == rl->line_pos)
        return;

    delete_from_line(rl, rl->line_pos - target, false, true);
}

static void do_delete_to_start_of_line(readline_t *rl) {
    rl->searching = false;
    delete_from_line(rl, rl->line_pos, false, true);
}

static void do_delete_to_end_of_line(readline_t *rl) {
    rl->searching = false;
    delete_from_line(rl, strlen(rl->line) - rl->line_pos, true, true);
}

static void do_yank_deleted_text(readline_t *rl) {
    rl->searching = false;
    insert_in_line(rl, rl->kill);
}

static void do_prev_history_entry(readline_t *rl) {
    rl->searching = false;
    if (rl->history_pos == -1)
        rl->history_pos = slist_size(rl->history);
    
    if (rl->history_pos == 0)
        return;

    rl->history_pos--;
    strcpy(rl->line, slist_get(rl->history, rl->history_pos));
    rl->line_pos = strlen(rl->line);
}

static void do_next_history_entry(readline_t *rl) {
    rl->searching = false;
    if (rl->history_pos == -1)
        rl->history_pos = slist_size(rl->history);
    
    if (rl->history_pos == slist_size(rl->history))
        return;
    
    // moving past the last history items, brings a new empty line
    rl->history_pos++;
    if (rl->history_pos == slist_size(rl->history)) {
        rl->line[0] = '\0';
    } else if (rl->history_pos < slist_size(rl->history)) {
        strcpy(rl->line, slist_get(rl->history, rl->history_pos));
    }
    rl->line_pos = strlen(rl->line);
}

static void do_incremental_search(readline_t *rl, bool backwards) {
    if (!rl->searching) {
        rl->searching = true;
        rl->searching_backwards = backwards;
        rl->search_term[0] = '\0';
        rl->search_failed = false;
    } else {
        perform_incremental_search(rl);
    }
}

static void do_complete_word(readline_t *rl) {
    find_word_to_cursor(rl);
    if (strlen(rl->word) == 0)
        return;

    int count = slist_count_prefix(rl->keywords, rl->word);
    if (count == 1) {
        int index = slist_indexof_prefix(rl->keywords, rl->word, 0);
        char *completed = slist_get(rl->keywords, index);
        insert_in_line(rl, completed + strlen(rl->word));
        rl->completion_list_pending = false;
    } else if (count > 1) {
        if (rl->completion_list_pending) {
            do_list_completions(rl);
        } else {
            rl->completion_list_pending = true;
        }
    }
}

static void do_list_completions(readline_t *rl) {
    char buff[80];
    find_word_to_cursor(rl);
    if (strlen(rl->word) == 0)
        return;

    tty_write("\n");
    int start = 0;
    int pos = slist_indexof_prefix(rl->keywords, rl->word, start);
    while (pos != -1) {
        sprintfn(buff, sizeof(buff), "> %s\n", slist_get(rl->keywords, pos));
        tty_write(buff);
        start = pos + 1;
        pos = slist_indexof_prefix(rl->keywords, rl->word, start);
    }

    tty_get_cursor(&rl->cursor_row, NULL);
}

static void do_insert_completions(readline_t *rl) {
    find_word_to_cursor(rl);
    if (strlen(rl->word) == 0)
        return;

    delete_from_line(rl, strlen(rl->word), false, false);
    int start = 0;
    int pos = slist_indexof_prefix(rl->keywords, rl->word, start);
    while (pos != -1) {
        insert_in_line(rl, slist_get(rl->keywords, pos));
        insert_in_line(rl, " ");
        start = pos + 1;
        pos = slist_indexof_prefix(rl->keywords, rl->word, start);
    }

    tty_get_cursor(&rl->cursor_row, NULL);
}

static void do_handle_printable(readline_t *rl, uint8_t character) {
    if (rl->searching) {
        // if we are searching, add but also search, otherwise, insert/append at position
        int len = strlen(rl->search_term);
        if (strlen(rl->search_term) >= (int)sizeof(rl->search_term) - 2)
            return;
        
        rl->search_term[len] = character;
        rl->search_term[len + 1] = '\0';
        rl->history_pos = -1;
        perform_incremental_search(rl);
    } else {
        // not searching, treat printable as letter
        char buffer[2];
        buffer[0] = character;
        buffer[1] = '\0';
        insert_in_line(rl, buffer);
    }
}

static void perform_incremental_search(readline_t *rl) {
    if (!rl->searching || strlen(rl->search_term) == 0)
        return;
    
    int start, pos;
    if (rl->searching_backwards) {
        start = (rl->history_pos == -1) ? slist_size(rl->history) - 1 : rl->history_pos - 1;
        pos = slist_last_indexof_containing(rl->history, rl->search_term, start);
    } else {
        start = (rl->history_pos == -1) ? 0 : rl->history_pos + 1;
        pos = slist_indexof_containing(rl->history, rl->search_term, start);
    }
    rl->search_failed = (pos == -1);
    if (!rl->search_failed) {
        rl->history_pos = pos;
        strcpy(rl->line, slist_get(rl->history, pos));
    }
}

