#include <stddef.h>
#include <stdbool.h>
#include "../multitask/process.h"
#include "../memory/kheap.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"
#include "../klog.h"
#include "../cpu.h"
#include "../string.h"
#include "../lock.h"

// this device is given or allocated by a task
// and the task can interact with the screen
// WHEN the tty is visible to the user
// some things will be automatic, such as scrolling both ways
// and switching terminals

// in theory, when the user calls "read" from the tty,
// until there is keyboard input, the task is blocked.
// also, when the user calls "write", the text goes to a buffer
// and is displayed on screen only if the tty is current, or switched to.

// how do we switch ttys? 
// how do we tell them which one is active?
// in linux one can ioctl() the stdin device and set the keyboard working mode
// the scancodes are converted into a unique key number between 1-127 
// (bit 7 is for key releases, zero means no key press)
// those numbers are used to lookup tables based on the shiftstate
// the result is printable characters or sequences of characters
    // int shift_final = shift_state ^ kbd->lockstate;
    // ushort *key_map = key_maps[shift_final];
    // keysym = key_map[keycode];

// very nice and thorogh explanation on tty drivers, devices, operations etc.
// also https://www.oreilly.com/library/view/linux-device-drivers/0596005903/ch18.html

// for an idea of how the linux kernel is doing it
// see https://kernel.org/doc/html/latest/tty/tty_struct.html#tty-struct-reference

// the tty driver registers the terminals
// also sets the termios flags (e.g. echo), for using the "serial port"



#define KEYS_BUFFER_SIZE  8

struct tty {
    struct tty *next;
    int dev_no;

    char *title;
    char *screen_buffer;
    int buffer_alloc_size;
    int first_visible_buffer_row;
    int total_buffer_rows;

    char row; // row num irrespective of first visible
    char column;
    char color;

    key_event_t keys_buffer[KEYS_BUFFER_SIZE];
    int keys_buffer_len;
    lock_t keys_buffer_lock;
};

typedef struct tty tty_t;

struct tty_mgr_data {
    int num_of_ttys; // e.g. 3
    tty_t *active_tty; // e.g. 0
    int header_lines; // to draw on top of every screen, Novel Netware style.
    struct tty *ttys_list;
};


static struct tty_mgr_data tty_mgr_data;
static void switch_to_tty(int dev_no);
static void scroll_tty_screenful(tty_t *tty, bool up);
static void enqueue_key_event(tty_t *tty, key_event_t *event);
static void dequeue_key_event(tty_t *tty, key_event_t *event);
static void virtual_buffer_put_buffer(tty_t *tty, char *buffer, int size, bool *need_screen_redraw);
static void draw_tty_buffer_to_screen(tty_t *tty);
static void handle_key_in_interrupt(key_event_t *event, bool *handled);
static void ensure_cursor_visible(tty_t *tty, bool *need_screen_redraw);


// essentially, this tty manager acts something like the window manager of the x-window sys
void init_tty_manager(int num_of_ttys, int lines_scroll_capacity) {
    memset(&tty_mgr_data, 0, sizeof(tty_mgr_data));
    tty_mgr_data.header_lines = 1;

    tty_t *tty;
    for (int i = 0; i < num_of_ttys; i++) {
        tty = kmalloc(sizeof(tty_t));
        memset(tty, 0, sizeof(tty_t));

        tty->dev_no = i;
        tty->total_buffer_rows = lines_scroll_capacity;
        tty->buffer_alloc_size = tty->total_buffer_rows * screen_cols() * 2;
        tty->screen_buffer = kmalloc(tty->buffer_alloc_size);

        // we cannot blindly memset(0), black on black makes cursor unviewable
        int offset = 0;
        while (offset < tty->buffer_alloc_size) {
            tty->screen_buffer[offset++] = ' ';
            tty->screen_buffer[offset++] = VGA_COLOR_LIGHT_GREY;
        }
        tty->color = VGA_COLOR_BLACK << 4 | VGA_COLOR_LIGHT_GREY;

        // insert it into the list of ttys
        tty->next = tty_mgr_data.ttys_list;
        tty_mgr_data.ttys_list = tty;
        tty_mgr_data.num_of_ttys++;
    }

    // we clear the screen and start the first tty
    screen_clear();
    switch_to_tty(0);
    keyboard_register_hook(handle_key_in_interrupt);
}

tty_t *tty_manager_get_device(int dev_no) {
    tty_t *tty = tty_mgr_data.ttys_list;
    while (tty != NULL) {
        if (tty->dev_no == dev_no)
            return tty;
        tty = tty->next;
    }
    return NULL;
}


/**
 * Interesting approach:
 * Have a global variable to indicate which process is the currently active process,
 * (the active, not currently executing), have the hardware driver append the keystroke 
 * to the StdIn for the active process. When the active process reads from stdin,
 * it will get the keys from its own stdin buffer.
 * from: https://wiki.osdev.org/Kernel_Stdio_Theory
 * Maybe the whole thing is to have a virtual terminal, with internal buffer
 * and a way of reflecting this on the screen.
 */

// called from an interrupt
static void handle_key_in_interrupt(key_event_t *event, bool *handled) {
    // if switching key, switch ttys and redraw them
    // if current is waiting for key, unblock it

    // but if the device never reads keyboard input, 
    // those keys should not go to other ttys...
    if (event->ctrl_down == 0
        && event->alt_down == 1
        && event->shift_down == 0
        && event->printable >= '1'
        && event->printable <= '9'
    ) {
        int tty_no = (event->printable - '1');
        if (tty_no < tty_mgr_data.num_of_ttys)
            switch_to_tty(tty_no);
    } else if (!event->ctrl_down
        && !event->alt_down
        && event->shift_down
        && (event->special_key == KEY_PAGE_UP || event->special_key == KEY_PAGE_DOWN)
    ) { 
        bool up = (event->special_key == KEY_PAGE_UP);
        scroll_tty_screenful(tty_mgr_data.active_tty, up);
    } else {
        // put in buffer
        enqueue_key_event(tty_mgr_data.active_tty, event);

        // if a process was blocked waiting for a key in this tty, unblock them.
        // if process was not blocked, no change will happen.
        unblock_process_that(WAIT_USER_INPUT, tty_mgr_data.active_tty);
    }
}

void tty_read_key(key_event_t *event) {
    tty_t *tty = running_process()->tty;
    if (tty == NULL)
        return;
    
    // if there are keys in the keys buffer, use them
    // other wise block on keyboard entry, whether we are or are not the current one
    if (tty->keys_buffer_len > 0) {
        dequeue_key_event(tty, event);
    } else {
        // current process getting blocked.
        klog_error("tty %d self-blocking on waiting for key", tty->dev_no);
        block_me(WAIT_USER_INPUT, tty);
        klog_error("tty %d got unblocked waiting for key", tty->dev_no);

        // if unblocked, it means we got a key!
        if (tty->keys_buffer_len == 0)
            klog_error("tty unblocked for key event, but no keys in buffer!");
        dequeue_key_event(tty, event);
    }
}

void tty_write(char *buffer) {
    tty_t *tty = running_process()->tty;
    if (tty != NULL)
        tty_write_specific_tty(tty, buffer);
}

void tty_write_specific_tty(tty_t *tty, char *buffer) {
    if (tty == NULL)
        return;
    
    // put something on the buffer of the tty 
    // if tty is visibile, put it on the screen as well    
    int len = strlen(buffer);
    //klog_trace("tty: writing %d bytes on tty %d", len, tty->dev_no);
    
    bool need_screen_redraw = false;
    virtual_buffer_put_buffer(tty, buffer, len, &need_screen_redraw);

    if (tty == tty_mgr_data.active_tty) {
        // TODO: optimize this later, to depict single line changes faster
        draw_tty_buffer_to_screen(tty);
    }
}

void tty_set_color(int color) {
    tty_t *tty = running_process()->tty;
    if (tty == NULL)
        return;
    
    // set the color, per tty that is.
    tty->color = color;
    if (tty_mgr_data.active_tty == tty)
        screen_set_color(color);
}

void tty_clear() {
    tty_t *tty = running_process()->tty;
    if (tty == NULL)
        return;

    // to make sure there is empty space on screen, 
    // add as many empty lines as needed in our buffer
    char newline = '\n';
    bool redraw = false;
    int visible_lines = (screen_rows() - tty_mgr_data.header_lines);
    int saved_row = tty->row;
    while (visible_lines-- > 0)
        virtual_buffer_put_buffer(tty, &newline, 1, &redraw);
    tty->row = saved_row + 1;
    tty->first_visible_buffer_row = tty->row;
    draw_tty_buffer_to_screen(tty);
}

void tty_get_cursor(uint8_t *row, uint8_t *col) {
    tty_t *tty = running_process()->tty;
    if (tty == NULL)
        return;
    
    if (row != NULL)
        *row = tty->row;
    if (col != NULL)
        *col = tty->column;
}

void tty_set_cursor(uint8_t row, uint8_t col) {
    tty_t *tty = running_process()->tty;
    if (tty == NULL)
        return;
    
    tty->row = row;
    tty->column = col;

    screen_set_cursor(
        tty_mgr_data.header_lines + tty->row - tty->first_visible_buffer_row,
        tty->column
    );
}

void tty_set_title(char *title) {
    tty_t *tty = running_process()->tty;
    if (tty == NULL)
        return;
    
    tty->title = title;
    if (tty == tty_mgr_data.active_tty)
        draw_tty_buffer_to_screen(tty);
}

void tty_set_title_specific_tty(tty_t *tty, char *title) {
    tty->title = title;
    if (tty == tty_mgr_data.active_tty)
        draw_tty_buffer_to_screen(tty);
}

static void enqueue_key_event(tty_t *tty, key_event_t *event) {
    if (tty->keys_buffer_len >= KEYS_BUFFER_SIZE) {
        klog_warn("Key buffer full for tty %d, dropping event", tty->dev_no);
        return;
    }
    klog_trace("tty: enqueueing key event on tty %d", tty->dev_no);
    acquire(&tty->keys_buffer_lock);
    memcpy(&tty->keys_buffer[tty->keys_buffer_len], event, sizeof(key_event_t));
    tty->keys_buffer_len++;
    release(&tty->keys_buffer_lock);
}

static void dequeue_key_event(tty_t *tty, key_event_t *event) {
    if (tty->keys_buffer_len == 0) {
        memset(event, 0, sizeof(key_event_t));
        return;
    }
    klog_trace("tty: dequeueing key event from tty %d", tty->dev_no);
    acquire(&tty->keys_buffer_lock);
    memcpy(event, &tty->keys_buffer[0], sizeof(key_event_t));
    tty->keys_buffer_len--;
    // shift everything one place up
    memmove(&tty->keys_buffer[0], &tty->keys_buffer[1], tty->keys_buffer_len * sizeof(key_event_t));
    release(&tty->keys_buffer_lock);
}

static void draw_tty_buffer_to_screen(tty_t *tty) {
    // this to be used when switching ttys and when scrolling
    // try to avoid flicker
    klog_trace("tty: drawing tty %d to screen", tty->dev_no);
    
    uint8_t header_color = (VGA_COLOR_BLUE << 4) | VGA_COLOR_LIGHT_CYAN;
    int row;

    // prepare the title area
    char buffer[32];
    for (row = 0; row < tty_mgr_data.header_lines; row++)
        screen_draw_full_row(' ', header_color, row);
    sprintfn(buffer, sizeof(buffer), "tty %d: ", tty->dev_no);
    screen_draw_str_at(buffer, header_color, 1, 0);
    screen_draw_str_at(
        tty->title == NULL ? "(untitled)" : tty->title, 
        header_color, 
        strlen(buffer) + 1, 
        0);
    sprintfn(buffer, sizeof(buffer), "row %d, first-visible %d", tty->row, tty->first_visible_buffer_row);
    screen_draw_str_at(buffer, header_color, 80 - strlen(buffer) - 1, 0);
    
    // copy the buffer to screen (either lines, or what fits on screen)
    int buffer_offset = tty->first_visible_buffer_row * screen_cols() * 2;
    int remaining_rows = (tty->buffer_alloc_size - buffer_offset) / (screen_cols() * 2);
    if (screen_rows() - row < remaining_rows)
        remaining_rows = screen_rows() - row;
    screen_copy_buffer_to_screen(tty->screen_buffer + buffer_offset,
        remaining_rows * screen_cols() * 2,
        0, row);
    row += remaining_rows;

    // clear the remaining screen
    while (row < screen_rows()) {
        screen_draw_full_row(' ', tty->color, row);
        row++;
    }

    screen_set_cursor(
        tty_mgr_data.header_lines + tty->row - tty->first_visible_buffer_row,
        tty->column
    );
}

static void scroll_tty_screenful(tty_t *tty, bool up) {
    int lines = screen_rows() - tty_mgr_data.header_lines - 1;

    if (up) {
        if (tty->first_visible_buffer_row - lines < 0)
            lines = tty->first_visible_buffer_row;
        tty->first_visible_buffer_row -= lines;
    } else {
        tty->first_visible_buffer_row += lines;
        int largest_first_visible = tty->total_buffer_rows - (screen_rows() - tty_mgr_data.header_lines);
        if (tty->first_visible_buffer_row > largest_first_visible)
            tty->first_visible_buffer_row = largest_first_visible;
    }
    draw_tty_buffer_to_screen(tty);
}

static void switch_to_tty(int dev_no) {
    klog_debug("tty: switching to tty %d", dev_no);

    tty_t *tty = tty_mgr_data.ttys_list;
    while (tty != NULL) {
        if (tty->dev_no == dev_no)
            break;
        tty = tty->next;
    }
    if (tty == NULL)
        return;

    tty_mgr_data.active_tty = tty;
    draw_tty_buffer_to_screen(tty);
    // should set cursor as well
    screen_set_color(tty->color);
}

// working with virtual term buffer, this converts special chars to screen behavior
static void virtual_buffer_put_buffer(tty_t *tty, char *buffer, int size, bool *need_screen_redraw) {
    // we work per buffer for optimization reasons
    // if less than 10 lines are remaining, scroll up 10 lines, adjust row/column accrodingly

    int offset = (tty->row * screen_cols() + tty->column) * 2;
    while (size-- > 0) {
        char c = *buffer++;
        switch (c) {
            case '\n':
                tty->column = 0;
                tty->row++;
                offset = (tty->row * screen_cols() + tty->column) * 2;
                break;

            case '\r':
                tty->column = 0;
                offset = (tty->row * screen_cols() + tty->column) * 2;
                break;

            case '\t':
                tty->column = ((tty->column >> 3) + 4) << 3;
                offset = (tty->row * screen_cols() + tty->column) * 2;
                break;

            case '\b':
                if (tty->column > 0)
                    tty->column--;
                offset = (tty->row * screen_cols() + tty->column) * 2;
                break;

            default:
                tty->screen_buffer[offset++] = c;
                tty->screen_buffer[offset++] = tty->color;
                if (++tty->column >= screen_cols()) {
                    tty->column = 0;
                    tty->row++;
                }
                break;
        }

        // our buffer will always allow more lines by losing some content up front.
        // we do this on a per line basis
        if (tty->row >= tty->total_buffer_rows) {
            // we need to scroll
            int line_size = screen_cols() * 2;
            memmove(
                tty->screen_buffer, 
                tty->screen_buffer + line_size,
                tty->buffer_alloc_size - line_size
            );
            memset(tty->screen_buffer + tty->buffer_alloc_size - line_size, 0, line_size);
            tty->row--;
            offset = (tty->row * screen_cols() + tty->column) * 2;
            *need_screen_redraw = true;
        }
    }

    ensure_cursor_visible(tty, need_screen_redraw);
}

// make sure "first_visible_buffer_row" is correct 
// and allows us to see the cursor on screen
static void ensure_cursor_visible(tty_t *tty, bool *need_screen_redraw) {

    // if our virtual row went below the visible viewport,
    // we have to update the viewport
    int visible_lines = (screen_rows() - tty_mgr_data.header_lines);
    if (tty->row >= tty->first_visible_buffer_row + visible_lines) {
        tty->first_visible_buffer_row = tty->row - visible_lines + 1;
        *need_screen_redraw = true;
    }

    if (tty->row < tty->first_visible_buffer_row) {
        tty->first_visible_buffer_row = tty->row;
        *need_screen_redraw = true;
    }
}
