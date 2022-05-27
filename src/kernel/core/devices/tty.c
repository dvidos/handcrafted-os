#include <stddef.h>
#include <stdbool.h>
#include "../multitask/process.h"
#include "../memory/kheap.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"
#include "../klog.h"
#include "../cpu.h"
#include "../string.h"

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
    int screen_buffer_alloc_size;
    int screen_buffer_length;
    int first_visible_buffer_row;

    char row;
    char col;
    char color;

    key_event_t keys_buffer[KEYS_BUFFER_SIZE];
    int keys_buffer_len;
};

typedef struct tty tty_t;

struct tty_mgr_data {
    int num_of_ttys; // e.g. 3
    tty_t *current_tty; // e.g. 0
    int header_lines;

    struct tty *ttys_list;
};

static struct tty_mgr_data tty_mgr_data;
static void switch_to_tty(int num);
static void scroll_tty(tty_t *tty, bool up);
static void enqueue_key_event(tty_t *tty, key_event_t *event);
static void dequeue_key_event(tty_t *tty, key_event_t *event);




// essentially, this tty manager acts something like the window manager of the x-window sys
void init_tty_manager(int num_of_ttys, int lines_scroll_capacity) {
    memset(&tty_mgr_data, 0, sizeof(tty_mgr_data));

    tty_t *tty;
    for (int i = 0; i < num_of_ttys; i++) {
        tty = kmalloc(sizeof(tty_t));
        memset(tty, 0, sizeof(tty_t));

        tty->dev_no = i;
        tty->screen_buffer_alloc_size = lines_scroll_capacity * screen_cols() * 2;
        tty->screen_buffer = kmalloc(tty->screen_buffer_alloc_size);

        // insert it into the list of ttys
        tty->next = tty_mgr_data.ttys_list;
        tty_mgr_data.ttys_list = tty;
    }

    screen_clear();
    switch_to_tty(0);
}

tty_t *tty_manager_get_device(int number) {
    tty_t *tty = tty_mgr_data.ttys_list;
    while (tty != NULL) {
        if (tty->dev_no == number)
            return tty;
        tty = tty->next;
    }
    return NULL;
}


// called from an interrupt
void tty_manager_key_handler_in_interrupt(key_event_t *event) {
    // if switching key, switch ttys and redraw them
    // if current is waiting for key, unblock it

    // but if the device never reads keyboard input, 
    // those keys should not go to other ttys...

    // ctrl+alt+Fn
    if (event->ctrl_down 
        && event->alt_down
        && !event->shift_down
        && event->printable == 0
        && event->special_key >= KEY_F1
        && event->special_key <= KEY_F12
    ) {
        int tty_no = (KEY_F1 - event->special_key);
        if (tty_no < tty_mgr_data.num_of_ttys)
            switch_to_tty(tty_no);
    } else if (!event->ctrl_down
        && !event->alt_down
        && event->shift_down
        && (event->special_key == KEY_PAGE_UP || event->special_key == KEY_PAGE_DOWN)
    ) { 
        bool up = (event->special_key == KEY_PAGE_UP);
        scroll_tty(tty_mgr_data.current_tty, up);
    } else {
        // we should also scroll into view...

        // put in buffer
        enqueue_key_event(tty_mgr_data.current_tty, event);

        // if a process was blocked waiting for a key in this tty, unblock them.
        // if process was not blocked, no change will happen.
        unblock_process_that(WAIT_USER_INPUT, tty_mgr_data.current_tty);
    }
}

void tty_read_key(tty_t *tty, key_event_t *event) {
    // if there are keys in the keys buffer, use them
    // other wise block on keyboard entry, whether we are or are not the current one
    if (tty->keys_buffer_len > 0) {
        dequeue_key_event(tty, event);
    } else {
        // current process getting blocked.
        block_me(WAIT_USER_INPUT, tty);

        // if unblocked, it means we got a key!
        if (tty->keys_buffer_len == 0)
            klog_error("tty unblocked for key event, but no keys in buffer");
        dequeue_key_event(tty, event);
    }
}

void tty_write(tty_t *tty, char *buffer, int len) {
    // put something on the buffer of the tty 
    // if tty is visibile, put it on the screen as well

    // we should manipulate our buffer.
    // advance our cursor as well.
    // our cursor should be in the virtual screen coordinates, not physical.
    // we should also treat special characters here, as well as scrolling etc.
}

void tty_set_color(tty_t *tty, int color) {
    // set the color, per tty that is.
    tty->color = color;
    if (tty_mgr_data.current_tty == tty)
        screen_set_color(color);
}

void tty_clear(tty_t *tty) {
    // if tty is visible, clear screen as well
}

static void enqueue_key_event(tty_t *tty, key_event_t *event) {
    if (tty->keys_buffer_len >= KEYS_BUFFER_SIZE) {
        klog_warn("Key buffer full for tty %d, dropping event", tty->dev_no);
        return;
    }
    pushcli();
    memcpy(&tty->keys_buffer[tty->keys_buffer_len], event, sizeof(key_event_t));
    tty->keys_buffer_len++;
    popcli();
}

static void dequeue_key_event(tty_t *tty, key_event_t *event) {
    if (tty->keys_buffer_len == 0) {
        memset(event, 0, sizeof(key_event_t));
        return;
    }
    pushcli();
    memcpy(event, &tty->keys_buffer[0], sizeof(key_event_t));
    tty->keys_buffer_len--;
    // shift everything one place up
    memmove(&tty->keys_buffer[0], &tty->keys_buffer[1], tty->keys_buffer_len * sizeof(key_event_t));
    popcli();
}

static void draw_tty_buffer_to_screen(tty_t *tty) {
    // this to be used when switching ttys and when scrolling
    // try to avoid flicker
    uint8_t header_color = (VGA_COLOR_BLUE << 4) | VGA_COLOR_LIGHT_CYAN;
    int row;

    // prepare the title area
    for (row = 0; row < tty_mgr_data.header_lines; row++)
        screen_draw_full_row(' ', header_color, row);
    if (tty->title != NULL)
        screen_draw_str_at(tty->title, header_color, 1, 0);
    
    // copy the buffer to screen (either lines, or what fits on screen)
    int buffer_offset = tty->first_visible_buffer_row * screen_cols() * 2;
    int remaining_rows = (tty->screen_buffer_alloc_size - buffer_offset) / (screen_cols() * 2);
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
}

static void scroll_tty(tty_t *tty, bool up) {

}

static void switch_to_tty(int num) {
    tty_t *tty = tty_mgr_data.ttys_list;
    while (num-- > 0 && tty != NULL)
        tty = tty->next;
    if (tty == NULL)
        return;

    tty_mgr_data.current_tty = tty;
    draw_tty_buffer_to_screen(tty);
    // should set cursor as well
    screen_set_color(tty->color);
}
