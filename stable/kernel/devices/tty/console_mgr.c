#include "console_mgr.h"
#include "vconsole.h"
#include "../drivers/kbd_drv.h"
#include "../drivers/screen.h"
#include "../include/uapi/key_codes.h"
#include "../include/uapi/key_event.h"
#include "../proc/process/process.h"
#include "../memory/kheap.h"
#include "../logger/logger.h"
#include "../arch/cpu.h"
#include "../klib/string.h"
#include "../utils/mutex.h"
#include "../utils/assert.h"

MODULE("TTY_MGR", LOG_LEVEL_INFO);


static void on_vconsole_modified(vconsole_t *vc);
static void on_key_event_occured(key_event_t *event, bool *handled);
static void switch_to_vconsole(int dev_no);


#define VGA_TEXT_BUFFER_ADDR 0xB8000
#define DEFAULT_ROWS 25
#define DEFAULT_COLS 80


struct console_mgr_data {
    int num_of_vconsoles; // e.g. 3
    int header_lines; // to draw on top of every screen, Novel Netware style.

    vconsole_t *active_vconsole; // e.g. 0
    vconsole_t *vconsoles_list;
};

static struct console_mgr_data console_mgr_data;





void init_console_mgr(int num_of_vconsoles) {
    ASSERT(num_of_vconsoles >= 1);

    memset(&console_mgr_data, 0, sizeof(console_mgr_data));
    console_mgr_data.header_lines = 1;

    vconsole_t *vc;
    for (int i = 0; i < num_of_vconsoles; i++) {
        vc = create_vconsole(DEFAULT_ROWS, DEFAULT_COLS, on_vconsole_modified);
        vc->next = console_mgr_data.vconsoles_list;
        console_mgr_data.vconsoles_list = vc;
        console_mgr_data.num_of_vconsoles++;
    }

    keyboard_register_hook(on_key_event_occured);
    screen_clear();
    switch_to_vconsole(0);
}

vconsole_t *console_mgr_get_vconsole(int dev_no) {
    vconsole_t *vc = console_mgr_data.vconsoles_list;
    for (int i = 0; i < dev_no && vc != NULL; i++)
        vc = vc->next;
    return vc;
}



static void show_vconsole_on_screen(vconsole_t *vc) {
    // struct vconsole_data *vd = (struct vconsole_data *)vc->data; // No longer direct access
    int screen_rows;
    int screen_cols;
    void *live_buffer_ptr;
    
    vc->ops->get_size(vc, &screen_rows, &screen_cols);
    
    int history_count = vc->ops->get_history_count(vc);
    int view_offset = vc->ops->get_view_offset(vc);

    // Determine source buffer based on view_offset
    if (view_offset == 0) {
        // Viewing live screen
        vc->ops->get_buffer_address(vc, &live_buffer_ptr);
        memcpy((void *)VGA_TEXT_BUFFER_ADDR, live_buffer_ptr, screen_rows * screen_cols * 2);
    } else {
        // Viewing history. Copy historical lines directly to VGA memory.
        uint8_t screen_color;
        vc->ops->get_text_attr(vc, &screen_color); // Get default color for blanking

        // The topmost line displayed will be at history index `history_count - view_offset - screen_rows`.
        int start_history_idx_to_display = history_count - view_offset;
        if (start_history_idx_to_display < 0) start_history_idx_to_display = 0;
        
        start_history_idx_to_display -= screen_rows;
        if (start_history_idx_to_display < 0) start_history_idx_to_display = 0;


        for (int i = 0; i < screen_rows; i++) {
            uint16_t *src_line;
            int current_history_line_relative_idx = start_history_idx_to_display + i;

            if (current_history_line_relative_idx >= 0 && current_history_line_relative_idx < history_count) {
                src_line = vc->ops->get_history_line(vc, current_history_line_relative_idx);
                if (src_line) {
                    memcpy((void *)(VGA_TEXT_BUFFER_ADDR + (i * screen_cols * 2)), src_line, screen_cols * sizeof(uint16_t));
                } else {
                    // Fallback: fill with blanks
                    memset((void *)(VGA_TEXT_BUFFER_ADDR + (i * screen_cols * 2)), ' ' | (screen_color << 8), screen_cols * sizeof(uint16_t));
                }
            } else {
                // Fill with blanks.
                memset((void *)(VGA_TEXT_BUFFER_ADDR + (i * screen_cols * 2)), ' ' | (screen_color << 8), screen_cols * sizeof(uint16_t));
            }
        }
    }
    
    // Cursor handling remains the same, but only for live view
    if (view_offset == 0) {
        int cursor_row;
        int cursor_col;
        bool cursor_visible;

        vc->ops->get_pos(vc, &cursor_row, &cursor_col);
        vc->ops->get_cursor_visible(vc, &cursor_visible);

        if (cursor_visible) {
            screen_set_cursor(cursor_row, cursor_col);
            screen_show_cursor();
        } else {
            screen_hide_cursor();
        }
    } else {
        // Hide cursor when viewing history
        screen_hide_cursor();
    }
}

static void switch_to_vconsole(int dev_no) {
    log_trace("switch_to_vconsole(%d)", dev_no);

    vconsole_t *vc = console_mgr_get_vconsole(dev_no);
    if (vc == NULL)
        return;
    
    console_mgr_data.active_vconsole = vc;
    show_vconsole_on_screen(console_mgr_data.active_vconsole);
}

static void on_vconsole_modified(vconsole_t *vc) {
    if (vc != console_mgr_data.active_vconsole)
        return;
    show_vconsole_on_screen(vc);
}

void vconsole_log_appender(void *context, const char *str) {
    // context is supposed to be a vconsole_t pointer
    vconsole_t *vc = (vconsole_t *)context;
    if (vc && vc->ops && vc->ops->puts)
        vc->ops->puts(vc, str);
}

static void on_key_event_occured(key_event_t *event, bool *handled) {

    uint16_t k = event->keycode;
    vconsole_t *vc = console_mgr_data.active_vconsole;
    if (vc == NULL) {
        *handled = false;
        return;
    }
    // struct vconsole_data *vd = (struct vconsole_data *)vc->data; // No longer direct access

    int screen_rows;
    int screen_cols;
    vc->ops->get_size(vc, &screen_rows, &screen_cols);

    int history_count = vc->ops->get_history_count(vc);
    int view_offset = vc->ops->get_view_offset(vc);

    // Handle TTY switching (Alt+Fn keys)
    if (k >= KEY_ALT_1 && k <= KEY_ALT_9) {
        int tty_no = (k - KEY_ALT_1);
        if (tty_no < console_mgr_data.num_of_vconsoles) {
            switch_to_vconsole(tty_no);
            *handled = true;
            return;
        }
    } 
    // Handle scrolling (Shift+PgUp/PgDown)
    else if (k == KEY_SHIFT_PAGE_UP || k == KEY_SHIFT_PAGE_DOWN) {
        if (k == KEY_SHIFT_PAGE_UP) {
            view_offset += screen_rows; // Scroll up one screenful
            if (view_offset > history_count) {
                view_offset = history_count; // Clamp to max history
            }
        } else { // KEY_SHIFT_PAGE_DOWN
            view_offset -= screen_rows; // Scroll down one screenful
            if (view_offset < 0) {
                view_offset = 0; // Clamp to live view
            }
        }
        vc->ops->set_view_offset(vc, view_offset); // Update internal offset

        // Immediately redraw the screen with the new scroll offset
        show_vconsole_on_screen(vc);
        *handled = true;
        return;
    } 
    // Handle any other key: if currently scrolled, reset to live view
    else {
        if (view_offset > 0) {
            vc->ops->set_view_offset(vc, 0); // Reset to live view
            show_vconsole_on_screen(vc); // Re-render to live view
        }
        log_trace("enqueueing key event 0x%x (%c)", event->keycode, event->ascii);
        vc->ops->enqueue_key(vc, event);

        // if a process was blocked waiting for a key in this tty, unblock them.
        unblock_process_that(WAIT_USER_INPUT, vc);
    }
    *handled = true; // Mark event as handled if it reached here and wasn't for TTY switch or scrolling
}
