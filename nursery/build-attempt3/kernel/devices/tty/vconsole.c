#include "../../include/uapi/base.h"
#include "../../klib/string.h"
#include "../../klib/list.h" // For potential future use, though not directly in this implementation
#include "../../memory/kheap.h" // For kmalloc and kfree
#include "../../utils/assert.h"
#include "../../drivers/kbd_drv.h"
#include "../../utils/mutex.h"
#include "../../logger/logger.h"
#include "../../proc/process/process.h"
#include "../../utils/assert.h"
#include "vconsole.h"
#include "../../include/uapi/key_codes.h"

MODULE("VCONS", LOG_LEVEL_INFO);


#define KEYS_QUEUE_SIZE          16
#define CANONICAL_BUFFER_SIZE   1024
#define CHAR_CTRL_C             0x03   // kill
#define CHAR_CTRL_D             0x04   // eof
#define CHAR_BACKSPACE          0x08   // 0x7F in some keyboards
#define CHAR_CTRL_U             0x15   // clear


struct vconsole_data {
    char *title;
    uint32_t flags;  // see vconsole_flag_t
    char *canonical_buffer;

    struct screen {
        struct size {
            int rows;
            int cols;
        } size;
        struct pos {
            int row;
            int col;
            bool visible;
        } cursor;
        uint8_t color;
        uint8_t buffer_num;
        struct scroll_lines {
            int begin;
            int end; // exclusive
        } scroll_lines;
    } screen;

    struct history {
        uint16_t *rows_array;
        int max_rows; // array capacity
        int count;    // rows stored
        int head;     // next write position
    } history;

    int current_buff_no;
    uint16_t *buffers[2];
    int view_offset; // for history?

    struct keys {
        key_event_t queue[KEYS_QUEUE_SIZE];
        int length;
    } keys;


    struct callbacks {
        console_buffer_modified_func *on_modified;
    } callbacks;
    int notification_counter;
};

#define vc_data(vconsole_ptr)    ((struct vconsole_data *)(vconsole_ptr)->data)


static inline void notify_modified(vconsole_t *vc) {
    if (vc_data(vc)->notification_counter <= 0) {
        vc_data(vc)->callbacks.on_modified(vc);
    }
}

static inline void push_notifications(vconsole_t *vc) {
    vc_data(vc)->notification_counter++;
}

static inline void pop_notifications(vconsole_t *vc) {
    vc_data(vc)->notification_counter--;
    notify_modified(vc);
}

static uint16_t *get_screen_ptr(struct vconsole_data *vd, int row, int col) {
    if (row < 0 || row >= vd->screen.size.rows || col < 0 || col >= vd->screen.size.cols)
        return NULL;
    
    return (vd->buffers[vd->current_buff_no] + (row * vd->screen.size.cols + col));
}

static void history_add_line(vconsole_t *vc, uint16_t *line_data) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    uint16_t *dest_line_ptr;

    // Calculate the destination in the circular buffer
    dest_line_ptr = vd->history.rows_array + (vd->history.head * vd->screen.size.cols);
    memcpy(dest_line_ptr, line_data, vd->screen.size.cols * sizeof(uint16_t));

    vd->history.head = (vd->history.head + 1) % vd->history.max_rows;

    if (vd->history.count < vd->history.max_rows) {
        vd->history.count++;
    }
}

static void scroll_up(vconsole_t *vc, int count) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (count <= 0 || count >= vd->screen.size.rows) {
        return; // nothing to scroll or scroll too much
    }

    // Save the lines that are about to scroll off the top to history
    for (int i = 0; i < count; i++) {
        uint16_t *line_to_save = get_screen_ptr(vd, vd->screen.scroll_lines.begin + i, 0);
        history_add_line(vc, line_to_save);
    }

    // move lines up
    uint16_t *src  = get_screen_ptr(vd, vd->screen.scroll_lines.begin + count, 0);
    uint16_t *dest = get_screen_ptr(vd, vd->screen.scroll_lines.begin, 0);
    size_t bytes_to_copy = (vd->screen.scroll_lines.end - vd->screen.scroll_lines.begin - count) * vd->screen.size.cols * sizeof(uint16_t);
    if (bytes_to_copy > 0)
        memcpy(dest, src, bytes_to_copy);

    // clear the newly blanked lines at the bottom of the scroll region
    uint16_t *clear_start = get_screen_ptr(vd, vd->screen.scroll_lines.end - count, 0);
    uint16_t blank = ' ' | (vd->screen.color << 8);
    for (int i = 0; i < count * vd->screen.size.cols; i++) {
        clear_start[i] = blank;
    }
}

static void shift_left_char(vconsole_t *vc, int row, int col) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (row < 0 || row >= vd->screen.size.rows || col < 0 || col >= vd->screen.size.cols - 1) {
        return; // Invalid position or cannot shift further left
    }

    // move things left
    uint16_t *src  = get_screen_ptr(vd, row, col + 1);
    uint16_t *dest = get_screen_ptr(vd, row, col);
    size_t bytes_to_copy = (vd->screen.size.cols - 1 - col) * sizeof(uint16_t);
    if (bytes_to_copy > 0)
        memcpy(dest, src, bytes_to_copy);
    
    // clear the last character in the line
    uint16_t *last_char = get_screen_ptr(vd, row, vd->screen.size.cols - 1);
    uint16_t blank = ' ' | (vd->screen.color << 8);
    *last_char = blank;
}

static void insert_line(vconsole_t *vc, int row) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (row < 0 || row >= vd->screen.size.rows) {
        return; // Invalid row
    }
    
    // Shift content down from the insert row to the end of the scroll region
    uint16_t *src =  get_screen_ptr(vd, row + 0, 0);
    uint16_t *dest = get_screen_ptr(vd, row + 1, 0);
    size_t bytes_to_copy = (vd->screen.scroll_lines.end - row - 1) * vd->screen.size.cols * sizeof(uint16_t);
    if (bytes_to_copy > 0)
        memmove(dest, src, bytes_to_copy); // use memmove for overlapping regions

    // Clear the newly inserted blank line
    uint16_t *clear_start = get_screen_ptr(vd, row, 0);
    uint16_t blank = ' ' | (vd->screen.color << 8);
    for (int i = 0; i < vd->screen.size.cols; i++) {
        clear_start[i] = blank;
    }
}



static void vconsole_clear(vconsole_t *vc) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    push_notifications(vc);
    uint16_t blank = ' ' | (vd->screen.color << 8);
    uint16_t *screen = get_screen_ptr(vd, 0, 0);
    for (int i = 0; i < vd->screen.size.rows * vd->screen.size.cols; i++) {
        screen[i] = blank;
    }
    vd->screen.cursor.row = 0;
    vd->screen.cursor.col = 0;
    pop_notifications(vc);
}

static void vconsole_putc(vconsole_t *vc, char c) {
    push_notifications(vc);
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    uint16_t *screen_mem;

    // Reset view_offset to 0 when new output arrives
    vd->view_offset = 0;

    switch (c) {
        case '\n': // Newline
            vd->screen.cursor.col = 0;
            vd->screen.cursor.row++;
            break;
        case '\r': // Carriage return
            vd->screen.cursor.col = 0;
            break;
        case '\b': // Backspace
            if (vd->screen.cursor.col > 0) {
                vd->screen.cursor.col--;
                screen_mem = get_screen_ptr(vd, vd->screen.cursor.row, vd->screen.cursor.col);
                *screen_mem = ' ' | (vd->screen.color << 8);
            }
            break;
        default:
            screen_mem = get_screen_ptr(vd, vd->screen.cursor.row, vd->screen.cursor.col);
            *screen_mem = c | (vd->screen.color << 8);
            vd->screen.cursor.col++;
            break;
    }

    // Handle end of line
    if (vd->screen.cursor.col >= vd->screen.size.cols) {
        vd->screen.cursor.col = 0;
        vd->screen.cursor.row++;
    }

    // Handle scrolling
    if (vd->screen.cursor.row >= vd->screen.scroll_lines.end) {
        scroll_up(vc, 1);
        vd->screen.cursor.row = vd->screen.scroll_lines.end - 1;
    }

    pop_notifications(vc);
}

static void vconsole_puts(vconsole_t *vc, const char *str) {
    if (!str) return;
    push_notifications(vc);
    for (int i = 0; str[i] != '\0'; i++) {
        vc->ops->putc(vc, str[i]);
    }
    pop_notifications(vc);
}

static void read_key_blocking(vconsole_t *vc, key_event_t *key) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    while (vd->keys.length == 0) {
        log_trace("vconsole[%p] sleeping on user input", vc);
        // assuming the running process is holding this tty
        proc_block(running_process(), WAIT_USER_INPUT, vc);
    }

    ASSERT(vd->keys.length > 0);
    *key = vd->keys.queue[0]; // copy value
    vd->keys.length--;
    memmove(vd->keys.queue, vd->keys.queue + 1, vd->keys.length * sizeof(key_event_t));
}

static int read_in_canonical_mode(vconsole_t *vc, char *buff, int size) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    key_event_t event;
    int bytes_read = 0;
    int current_len = strlen(vd->canonical_buffer); // Initialize length once

    while (true) {
        read_key_blocking(vc, &event);
        char c = event.ascii;
        
        if (c == 0) {
            // Ignore key events that don't have an ASCII representation
            continue;
        }

        // Handle CR_TO_LF transformation if enabled
        if (c == '\r' && (vd->flags & CR_TO_LF)) {
            c = '\n';
        }

        switch (c) {
            case CHAR_CTRL_C: // Ctrl-C (0x03) - Clear line, don't return characters
                if (vd->flags & ECHO) {
                    // Visually clear the line
                    for (int i = 0; i < current_len; i++) {
                        vconsole_putc(vc, '\b');
                        vconsole_putc(vc, ' ');
                        vconsole_putc(vc, '\b');
                    }
                }
                vd->canonical_buffer[0] = '\0'; // Clear internal buffer
                current_len = 0; // Reset length
                break;

            case CHAR_CTRL_D: // Ctrl-D (0x04) - EOF or line terminator
                if (current_len == 0) {
                    // If buffer is empty, return 0 for EOF
                    return 0;
                } else {
                    // If buffer is not empty, treat Ctrl-D as a line terminator
                    // The current buffered data is returned.
                    goto return_buffered_data;
                }

            case CHAR_CTRL_U: // Ctrl-U (0x15) - Clear line
                if (vd->flags & ECHO) {
                    // Visually clear the line
                    for (int i = 0; i < current_len; i++) {
                        vconsole_putc(vc, '\b');
                        vconsole_putc(vc, ' ');
                        vconsole_putc(vc, '\b');
                    }
                }
                vd->canonical_buffer[0] = '\0'; // Clear internal buffer
                current_len = 0; // Reset length
                break;

            case CHAR_BACKSPACE: // Backspace (0x08 or 0x7F) - Erase character
                if (current_len > 0) {
                    current_len--; // Decrement length first
                    vd->canonical_buffer[current_len] = '\0'; // Null-terminate at new length
                    if (vd->flags & ECHO) {
                        vconsole_putc(vc, '\b');
                        vconsole_putc(vc, ' ');
                        vconsole_putc(vc, '\b');
                    }
                }
                break;

            case '\n': // Newline - Line terminator
                // Append newline to canonical buffer
                if (current_len + 1 < CANONICAL_BUFFER_SIZE) { // Check for space for char + null
                    vd->canonical_buffer[current_len++] = c;
                    vd->canonical_buffer[current_len] = '\0'; // Null-terminate
                }
                if (vd->flags & ECHO) {
                    vconsole_putc(vc, c); // Echo newline
                }
                goto return_buffered_data;

            default: // Regular character
                if (current_len + 1 < CANONICAL_BUFFER_SIZE) { // Check for space for char + null
                    vd->canonical_buffer[current_len++] = c;
                    vd->canonical_buffer[current_len] = '\0'; // Null-terminate
                    if (vd->flags & ECHO) {
                        vconsole_putc(vc, c); // Echo character
                    }
                } else {
                    // Buffer full, ignore character and potentially log a warning
                    log_warn("Canonical buffer full, ignoring character '%c'", c);
                }
                break;
        }
    }

return_buffered_data:
    // Copy data from canonical buffer to user's buffer
    bytes_read = current_len; // Use current_len for bytes_read
    if (bytes_read > size) {
        bytes_read = size; // Don't overflow user buffer
    }
    memcpy(buff, vd->canonical_buffer, bytes_read);
    vd->canonical_buffer[0] = '\0'; // Clear canonical buffer after returning its content
    // current_len will be re-initialized on next call or implicitly reset to 0 by the above line.

    return bytes_read;
}

static int read_in_raw_mode(vconsole_t *vc, char *buff, int size) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    key_event_t event;
    int bytes_read = 0;

    // In raw mode, we return each key immediately as it's read,
    // until the user's buffer is full or no more keys are available.
    // This allows for multi-byte sequences to be read as individual bytes.
    while (bytes_read < size) {
        // Attempt to read a key without blocking initially if there are keys in the queue
        if (vd->keys.length == 0) {
            // If queue is empty, block until a key arrives
            read_key_blocking(vc, &event);
        } else {
            // If keys are available, process them without blocking further
            event = vd->keys.queue[0];
            vd->keys.length--;
            memmove(vd->keys.queue, vd->keys.queue + 1, vd->keys.length * sizeof(key_event_t));
        }

        char c = event.ascii;
        if (c == 0) {
            // Ignore key events that don't have an ASCII representation
            continue;
        }

        // Handle CR_TO_LF transformation if enabled
        if (c == '\r' && (vd->flags & CR_TO_LF)) {
            c = '\n';
        }

        // Echo character if ECHO flag is set
        if (vd->flags & ECHO) {
            vconsole_putc(vc, c);
        }

        // Store the character in the user's buffer
        buff[bytes_read++] = c;

        // If this was the last key in the queue, and we have read at least one byte,
        // we can return early to prevent blocking for more input, unless the buffer isn't full.
        if (vd->keys.length == 0 && bytes_read > 0 && bytes_read < size) {
             break; // Return what we have
        }
    }

    return bytes_read;
}

static int vconsole_read(vconsole_t *vc, char *buff, int size) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (vd->flags & CANONICAL_MODE)
        return read_in_canonical_mode(vc, buff, size);
    else
        return read_in_raw_mode(vc, buff, size);
}

static void vconsole_write(vconsole_t *vc, const char *buff, int size) {
    if (!buff || size <= 0) return;
    push_notifications(vc);
    for (int i = 0; i < size; i++) {
        vc->ops->putc(vc, buff[i]);
    }
    pop_notifications(vc);
}

static void vconsole_set_pos(vconsole_t *vc, int row, int col) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (row < 0) row = 0;
    if (row >= vd->screen.size.rows) row = vd->screen.size.rows - 1;
    if (col < 0) col = 0;
    if (col >= vd->screen.size.cols) col = vd->screen.size.cols - 1;

    if (vd->screen.cursor.row != row || vd->screen.cursor.col != col) {
        vd->screen.cursor.row = row;
        vd->screen.cursor.col = col;
        notify_modified(vc);
    }
}

static void vconsole_get_pos(vconsole_t *vc, int *row, int *col) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (row) *row = vd->screen.cursor.row;
    if (col) *col = vd->screen.cursor.col;
}

static void vconsole_get_size(vconsole_t *vc, int *rows, int *cols) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (rows) *rows = vd->screen.size.rows;
    if (cols) *cols = vd->screen.size.cols;
}

static void vconsole_set_size(vconsole_t *vc, int rows, int cols) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (vd->screen.size.rows == rows && vd->screen.size.cols == cols)
        return;
    
    vd->screen.size.rows = rows;
    vd->screen.size.cols = cols;
    // we should reallocate buffers, redraw etc.
    notify_modified(vc);
}

static void vconsole_set_text_attr(vconsole_t *vc, uint8_t color) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    vd->screen.color = color;
}

static void vconsole_get_text_attr(vconsole_t *vc, uint8_t *color) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (color) *color = vd->screen.color;
}

static void vconsole_set_scroll_lines(vconsole_t *vc, int begin, int end) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (begin < 0) begin = 0;
    if (end > vd->screen.size.rows) end = vd->screen.size.rows;
    if (begin >= end) { // Invalid range, reset to full screen
        begin = 0;
        end = vd->screen.size.rows;
    }
    vd->screen.scroll_lines.begin = begin;
    vd->screen.scroll_lines.end = end;
}

static void vconsole_get_scroll_lines(vconsole_t *vc, int *begin, int *end) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (begin) *begin = vd->screen.scroll_lines.begin;
    if (end) *end = vd->screen.scroll_lines.end;
}

static void vconsole_set_alt_buffer(vconsole_t *vc, bool alt) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    int new_buff_no = alt ? 1 : 0;
    if (vd->current_buff_no != new_buff_no) {
        vd->current_buff_no = new_buff_no;
        notify_modified(vc);
    }
}

static void vconsole_get_alt_buffer(vconsole_t *vc, bool *alt) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (alt) *alt = (vd->current_buff_no == 1);
}

static void vconsole_set_cursor_visible(vconsole_t *vc, bool visible) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (vd->screen.cursor.visible != visible) {
        vd->screen.cursor.visible = visible;
        notify_modified(vc);
    }
}

static void vconsole_get_cursor_visible(vconsole_t *vc, bool *visible) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (visible) *visible = vd->screen.cursor.visible;
}

static void vconsole_get_buffer_address(vconsole_t *vc, void **address) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (address) *address = vd->buffers[vd->current_buff_no];
}

static void vconsole_set_title(vconsole_t *vc, char *title) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (strcmp(title, vd->title) != 0) {
        if (vd->title) kfree(title);
        vd->title = kstrdup(title);
        notify_modified(vc);
    }
}

static const char *vconsole_get_title(vconsole_t *vc) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    return vd->title;
}

static void vconsole_set_flag(vconsole_t *vc, vconsole_flag_t flag, bool enabled) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (enabled)
        vd->flags |= flag;
    else
        vd->flags &= ~flag;
}

static bool vconsole_get_flag(vconsole_t *vc, vconsole_flag_t flag) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    return (bool)(vd->flags & flag);
}

static void vconsole_enqueue_key(vconsole_t *vc, key_event_t *event) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (vd->keys.length >= KEYS_QUEUE_SIZE) {
        log_warn("enqueue_key_event(): buffer full, dropping key event");
        return;
    }
    
    vd->keys.queue[vd->keys.length] = *event;  // copy value
    vd->keys.length++;
}

static uint16_t *vconsole_get_history_line(vconsole_t *vc, int index) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (index < 0 || index >= vd->history.count) {
        return NULL; // Invalid index
    }
    int actual_index = (vd->history.head - vd->history.count + index + vd->history.max_rows) % vd->history.max_rows;
    return vd->history.rows_array + (actual_index * vd->screen.size.cols);
}

static int vconsole_get_history_count(vconsole_t *vc) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    return vd->history.count;
}

static int vconsole_get_view_offset(vconsole_t *vc) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    return vd->view_offset;
}

static void vconsole_set_view_offset(vconsole_t *vc, int offset) {
    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    vd->view_offset = offset;
}

static void vconsole_destroy(vconsole_t *vc) {
    if (vc == NULL)
        return;

    struct vconsole_data *vd = (struct vconsole_data *)vc->data;
    if (vd != NULL) {
        if (vd->canonical_buffer)   kfree(vd->canonical_buffer);
        if (vd->buffers[0])         kfree(vd->buffers[0]);
        if (vd->buffers[1])         kfree(vd->buffers[1]);
        if (vd->history.rows_array) kfree(vd->history.rows_array);
        kfree(vd);
    }

    kfree(vc);
}

static struct vconsole_ops ops = {
    .read = vconsole_read,
    .write = vconsole_write,

    .clear = vconsole_clear,
    .putc = vconsole_putc,
    .puts = vconsole_puts,

    .set_pos = vconsole_set_pos,
    .get_pos = vconsole_get_pos,
    .get_size = vconsole_get_size,
    .set_size = vconsole_set_size,
    .set_text_attr = vconsole_set_text_attr,
    .get_text_attr = vconsole_get_text_attr,
    .set_scroll_lines = vconsole_set_scroll_lines,
    .get_scroll_lines = vconsole_get_scroll_lines,
    .set_alt_buffer = vconsole_set_alt_buffer,
    .get_alt_buffer = vconsole_get_alt_buffer,
    .set_cursor_visible = vconsole_set_cursor_visible,
    .get_cursor_visible = vconsole_get_cursor_visible,
    .get_buffer_address = vconsole_get_buffer_address,

    .set_title = vconsole_set_title,
    .get_title = vconsole_get_title,

    .set_flag = vconsole_set_flag,
    .get_flag = vconsole_get_flag,

    
    .enqueue_key = vconsole_enqueue_key,
    .destroy = vconsole_destroy,
    
    .get_history_line = vconsole_get_history_line,
    .get_history_count = vconsole_get_history_count,
    .get_view_offset = vconsole_get_view_offset,
    .set_view_offset = vconsole_set_view_offset,
};

vconsole_t *create_vconsole(int rows, int cols, console_buffer_modified_func *on_modified) {
    vconsole_t *vc = (vconsole_t *)kmalloc(sizeof(vconsole_t));
    if (!vc) return NULL;

    struct vconsole_data *vd = (struct vconsole_data *)kmalloc(sizeof(struct vconsole_data));
    if (!vd) {
        kfree(vc);
        return NULL;
    }

    memset(vd, 0, sizeof(struct vconsole_data));

    vd->screen.size.rows = rows;
    vd->screen.size.cols = cols;
    vd->screen.cursor.row = 0;
    vd->screen.cursor.col = 0;
    vd->screen.color = vconsole_color(WHITE, BLACK, false, false); // Default white on black
    vd->screen.buffer_num = 0; // Main buffer
    vd->screen.scroll_lines.begin = 0;
    vd->screen.scroll_lines.end = vd->screen.size.rows;
    vd->screen.cursor.visible = true;

    vd->canonical_buffer = kmalloc(CANONICAL_BUFFER_SIZE);
    if (!vd->canonical_buffer) {
        kfree(vc);
        kfree(vd);
        return NULL;
    }
    vd->canonical_buffer[0] = 0;

    // Initialize history buffer (circular array of lines)
    vd->history.max_rows = vd->screen.size.rows * 2; // Store two full screens of history
    // Each line in history stores characters with attributes (uint16_t)
    vd->history.rows_array = kmalloc(vd->history.max_rows * vd->screen.size.cols * sizeof(uint16_t));
    if (!vd->history.rows_array) {
        kfree(vd->canonical_buffer);
        kfree(vc);
        kfree(vd);
        return NULL;
    }
    memset(vd->history.rows_array, 0, vd->history.max_rows * vd->screen.size.cols * sizeof(uint16_t));
    vd->history.count = 0; // No history lines stored yet
    vd->history.head = 0;  // Next write position in the circular buffer

    size_t screen_buffer_size = cols * rows * 2;
    vd->buffers[0] = kmalloc(screen_buffer_size);
    if (!vd->buffers[0]) {
        kfree(vd->history.rows_array);
        kfree(vd->canonical_buffer);
        kfree(vc);
        kfree(vd);
        return NULL;
    }
    vd->buffers[1] = kmalloc(screen_buffer_size);
    if (!vd->buffers[1]) {
        kfree(vd->buffers[0]);
        kfree(vd->history.rows_array);
        kfree(vd->canonical_buffer);
        kfree(vc);
        kfree(vd);
        return NULL;
    }

    vd->notification_counter = 0;
    vd->callbacks.on_modified = on_modified;

    vc->ops = &ops;
    vc->data = vd;

    // sensibly clear buffers
    vc->ops->set_alt_buffer(vc, true);
    vc->ops->clear(vc);
    vc->ops->set_alt_buffer(vc, false);
    vc->ops->clear(vc);

    // sensible defaults
    vc->ops->set_flag(vc, CANONICAL_MODE,  true);
    vc->ops->set_flag(vc, ECHO,            true);
    vc->ops->set_flag(vc, SIGNAL_HANDLING, true);
    vc->ops->set_flag(vc, CR_TO_LF,        true);
    vc->ops->set_flag(vc, FLOW_CONTROL,    false);
    vc->ops->set_flag(vc, LF_TO_CRLF,      true);
    
    return vc;
}

uint8_t vconsole_fg_color(uint8_t color, enum text_color fg, bool bright) {
    return (color & 0xF0) | (fg & 0x0F) | (bright ? 0x08 : 0x00);
}

uint8_t vconsole_bg_color(uint8_t color, enum text_color bg) {
    return (color & 0x0F) | ((bg & 0x07) << 4);
}

uint8_t vconsole_color(enum text_color fg, enum text_color bg, bool bright, bool blink) {
    uint8_t attr = 0;
    attr = (fg & 0x0F);
    if (bright) attr |= 0x08;
    attr |= ((bg & 0x07) << 4);
    if (blink) attr |= 0x80;
    return attr;
}
