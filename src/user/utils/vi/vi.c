
/**
 * If we are building an editor, ideally,
 * we should be building a multi-line buffer object.
 * It would offer methods to insert, delete, navigate.
 * And then, the only thing missing is keyboard/screen integration.
 * For example:
 */
struct multi_line_buffer_ops;
typedef struct multi_line_buffer {
    char *buffer;
    int buffer_size;
    int row_number;
    int col_number;
    int offset;
    int text_length;
    struct multi_line_buffer_ops *ops;
} mlb_t;
struct multi_line_buffer_ops {
    int (*navigate_char)(mlb_t *mlb, bool forward);
    int (*navigate_word)(mlb_t *mlb, bool forward);
    int (*navigate_line_boundaries)(mlb_t *mlb, bool forward);
    int (*navigate_buffer_boundaries)(mlb_t *mlb, bool forward);
    int (*navigate_line_vertically)(mlb_t *mlb, bool forward);
    int (*navigate_page_vertically)(mlb_t *mlb, bool forward);
    int (*navigate_to_line_no)(mlb_t *mlb, int line_no);

    int (*delete_char)(mlb_t *mlb, bool forward);
    int (*delete_word)(mlb_t *mlb, bool forward);
    int (*delete_while_line)(mlb_t *mlb);
    int (*delete_to_line_boundaries)(mlb_t *mlb, bool forward);

    int (*insert_char)(mlb_t *mlb, int chr);
    int (*insert_str)(mlb_t *mlb, char *text);
};
mlb_t *create_multi_line_buffer(int buffer_size);
void destroy_multi_line_buffer(mlb_t *mlb);

/**
 * Then, we'll have a viewport, a portion of the screen that we paint
 * This viewport will have the visible row and col offsets
 */
typedef struct multi_line_buffer_viewport {
    mlb_t *mlb;
    int visible_rows;
    int visible_cols;
    int first_visible_row; // of text, zero based
    int first_visible_col;
} mlbv_t;
struct multi_line_buffer_viewport_ops {
    int (*repaint)(mlbv_t *vp);
    int (*ensure_cursor_visible)(mlbv_t *vp);
}
mlbv_t *create_multi_line_buffer_viewport(mlbt_t *buffer);
void destroy_multi_line_buffer_viewport(mlbv_t *vp);


// #define KEY_BACKSPACE    0x7f
// #define KEY_ENTER        0x0a
// #define KEY_ESCAPE       0x1b

// #define LINUX_COMPATIBILITY 1
// #ifdef LINUX_COMPATIBILITY

//     //     #include <termios.h>
//     #include <unistd.h>
//     #include <stdio.h>
//     #include <string.h>

//     #define TERM_KEY_UP_ARROW     "\x1b[A"
//     #define TERM_KEY_DOWN_ARROW   "\x1b[B"
//     #define TERM_KEY_RIGHT_ARROW  "\x1b[C"
//     #define TERM_KEY_LEFT_ARROW   "\x1b[D"
//     #define TERM_KEY_HOME         "\x1b[H"
//     #define TERM_KEY_END          "\x1b[F"
//     #define TERM_KEY_PAGE_UP      "\x1b[5~"
//     #define TERM_KEY_PAGE_DOWN    "\x1b[6~"
//     #define TERM_KEY_DELETE       "\x1b[3~"
//     #define TERM_KEY_INSERT       "\x1b[2~"
//     #define TERM_KEY_BACKSPACE    0x7f
//     #define TERM_KEY_ENTER        0x0a
//     #define TERM_KEY_ESCAPE       0x1b

//     static struct termios old_termios; // used for updating linux terminal

//     void linux_entry() {
//         static struct termios newt;

//         /*tcgetattr gets the parameters of the current terminal
//         STDIN_FILENO will tell tcgetattr that it should write the settings
//         of stdin to oldt*/
//         tcgetattr( STDIN_FILENO, &old_termios);
//         /*now the settings will be copied*/
//         newt = old_termios;

//         /*ICANON normally takes care that one line at a time will be processed
//         that means it will return if it sees a "\n" or an EOF or an EOL*/
//         newt.c_lflag &= ~(ICANON | ECHO); 

//         /*Those new settings will be set to STDIN
//         TCSANOW tells tcsetattr to change attributes immediately. */
//         tcsetattr( STDIN_FILENO, TCSANOW, &newt);
//     }
//     void linux_exit() {
//         /*restore the old settings*/
//         tcsetattr( STDIN_FILENO, TCSANOW, &old_termios);
//     }
//     #define ON_ENTRY()  linux_entry()
//     #define ON_EXIT()   linux_exit()

//     // gotoxy() x,y are one based, we are zero based
//     #define gotoxy(x,y)  printf("\033[%d;%dH", (y) + 1, (x) + 1)

//     // it was too much trouble detecting the escape sequences
//     // i will do it in my os in my way (not a character tty)
//     int getkey() { 
//         return getchar();
//     }
//     #define screen_rows()    25
//     #define screen_cols()    80

// #else

//     // definitions for handcrafted-os
//     #define ON_ENTRY()  (0)
//     #define ON_EXIT()   (0)
//     #define gotoxy(x,y)    ((x)|(y))
//     #define getkey()    (0)

// #endif

// // ------------------------------------------------------------------

// #define MODE_COMMAND         0
// #define MODE_INSERT          1


// int mode = 0;
// int finished = 0;
// int row = 0;
// int col = 0;
// int first_visible_row = 0;
// int first_visible_col = 0;
// int offset = 0;
// int total_rows = 0;
// int multiplier = 1;
// char command[80 + 1] = {0,}; 
// char buffer[64 * 1024]; // 64K seems to be a good start for editing small files


// void insert_char_at_position(char key) {
//     // in theory we'll keep both row/col and offset in sync.
//     buffer[offset] = key;
//     offset++;
//     col++;
//     if (col >= first_visible_col + screen_cols()) {
//         first_visible_col++;
//         draw_screen();
//     } else {
//         draw_current_line();
//     }
// }
// void collect_command_multiplier(int key) {
//     if (key >= '0' && key <= '9')
//         multiplier = (multiplier * 10) + (key - '0');
// }
// void navigate_up() {
//     while (multiplier-- > 0) {

//     }
// }
// void navigate_down() {
// }
// void navigate_character_left() {
// }
// void navigate_character_right() {
// }
// void navigate_word_left() {
// }
// void navigate_word_right() {
// }
// void navigate_start_of_line() {
// }
// void navigate_end_of_line() {
// }
// void navigate_start_of_document() {
// }
// void navigate_end_of_document() {
// }
// void navigate_to_line() {
// }
// void delete_character_right() {
// }
// void delete_character_left() {
// }
// void delete_word_right() {
// }
// void delete_word_left() {
// }
// void delete_line() {
// }
// void insert_character(int key) {
// }
// void search(char *needle, bool forwards) {
// }
// void find_next(bool forwards) {
// }

// // ------------------------------------
// // screen update section
// // ------------------------------------

// // draw only the currently edited line (for speed)
// void draw_current_line() {
//     ; // gotoxy(), draw the line, gotoxy()
// }

// // draw the footer line
// void draw_footer() {
//     gotoxy(0, screen_rows() - 1);
//     if (mode == MODE_COMMAND) {
//         printf("%79s", " ");
//     } else if (mode == MODE_INSERT) {
//         printf("--- INSERT ---      %d,%d     %d%%", row, col, 
//             total_rows == 0 ? 0 : first_visible_row / total_rows);
//     }
// }

// // draw the whole screen anew
// void draw_screen() {
//     for (int row = 0; row < screen_rows() - 1; row++) {
//         gotoxy(0, row);
//         if (row < total_rows)
//             printf("%-80s", "");
//         else
//             printf("%-80s", "~");
//     }
//     // now go where we are supposed to be
//     gotoxy(col - first_visible_col, row - first_visible_row);
// }

// // run the command line (ex mode)
// void execute_command_line() {
//     printf("Executing command [%s]\n", command);
//     // should probably do it char by char, but for now...
//     if (strcmp(command, ":q") == 0) {
//         finished = true;
//     }
//     /*
//         r  read file (filename as argument)
//         w  write file (optional filename as argument)
//         w - write
//         q - quit
//         s  replace
        
//     */
// }

// // edit the command line, first char can be ':' or '/'
// bool edit_command_line(int first_char) {
//     int cmd_len = 0;
//     bool cmd_done = 0;

//     command[0] = first_char;
//     command[1] = '\0';
//     while (!cmd_done) {
//         // draw command line
//         cmd_len = strlen(command);
//         gotoxy(0, screen_rows() - 1);
//         printf("%-79s", command);
//         gotoxy(cmd_len, screen_rows() - 1);

//         int key = getchar();
//         switch (key) {
//             case KEY_ENTER:
//                 return true;
//             case KEY_BACKSPACE:
//                 if (cmd_len > 0)
//                     command[--cmd_len] = '\0';
//                 if (cmd_len == 0)
//                     return false;
//                 break;
//             case KEY_ESCAPE:
//                 return false;
//             default:
//                 if (key >= ' ' && key <= 127 && cmd_len < 80) {
//                     command[cmd_len++] = key;
//                     command[cmd_len] = '\0';
//                 }
//         }
//     }
// }

// bool handle_all_modes_key(int key) {
//     // for example, arrow keys will work in both modes
//     // return true if key was a any_mode key
//     switch (key) {
//         // arrow keys
//         // delete
//         // page up/down
//         // home end
//     }
//     return false;
// }

// void handle_command_key(int key) {
//     // e.g. navigation, deletion, virtual, yank, numbers, etc
//     switch (key) {
//         case 'i':
//             mode = MODE_INSERT;
//             draw_footer();
//             break;
//         case ':':
//         case '/':
//             bool confirmed = edit_command_line(key);
//             draw_footer();
//             if (confirmed)
//                 execute_command_line();
//             break;
//     }
//     /*
//         h,j,k,l navigation
//         0 start of line
//         $ end of line
//         w go to start of next word
//         b go to start of prev word
//         u undo
//         i insert before cursor
//         I insert at start of line
//         a append after cursor
//         A append at end of line
//         o open and insert in new line below the cursor
//         O open and insert in new line above the cursor
//         r replace single character
//         R replace multiple characters (it's a mode, esc to exit)
//         C clear to end of line, then enter insert mode
//         cc clear all line, then enter insert mode
//         x  delete character
//         dw delete word to right
//         D  delete remainder of line
//         dd delete entire line
//         yy yank the current line
//         p  paste the yanked line after current line
//         P  paste the yanked line before current line
//         /  search forwards
//         ?  search backwards
//         n  find next
//         N  find previous
//         nG go to line number n
//         GG go to start of the file
//         ^G go to last line of file
//     */
// }

// void handle_insert_key(int key) {
//     // insert the character, redraw line or page, depending
//     switch (key) {
//         case KEY_ESCAPE:
//             mode = MODE_COMMAND;
//             draw_footer();
//             break;
//         // the rest we'll insert I think
//     }
// }




// void main() {
//     ON_ENTRY();

//     draw_screen();
//     while (!finished) {
//         int key = getchar();
//         if (handle_all_modes_key(key)) {
//             continue;
//         } else if (mode == MODE_COMMAND) {
//             handle_command_key(key);
//         } else if (mode == MODE_INSERT) {
//             handle_insert_key(key);
//         }
//     }

//     ON_EXIT();
// }


// // ------------------------------------------------------------


