#include <uapi/base.h>
#include <klib/string.h>
#include <klib/cpu_tools.h> // For outb, inb - needed for update_hardware_cursor
#include <klib/list.h> // For potential future use, though not directly in this implementation
#include <memory/kheap.h> // For kmalloc and kfree
#include "text_screen.h"

// Assuming a standard VGA text mode buffer address and dimensions
#define VGA_TEXT_BUFFER_ADDR 0xB8000
#define DEFAULT_SCREEN_ROWS 25
#define DEFAULT_SCREEN_COLS 80

// Private helper functions
static uint16_t *get_screen_ptr(text_screen_t *s, int row, int col) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    if (row < 0 || row >= priv->rows || col < 0 || col >= priv->cols) {
        return NULL; // Should not happen with proper bounds checking before call
    }
    return (uint16_t *)(VGA_TEXT_BUFFER_ADDR + (row * priv->cols + col) * sizeof(uint16_t));
}

static void scroll_up(text_screen_t *s, int count) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    if (count <= 0 || count >= priv->rows) {
        return; // Nothing to scroll or scroll too much
    }

    uint16_t *dest = (uint16_t *)VGA_TEXT_BUFFER_ADDR + priv->state.scroll_lines.begin * priv->cols;
    uint16_t *src = (uint16_t *)VGA_TEXT_BUFFER_ADDR + (priv->state.scroll_lines.begin + count) * priv->cols;
    size_t bytes_to_copy = (priv->state.scroll_lines.end - priv->state.scroll_lines.begin - count) * priv->cols * sizeof(uint16_t);

    if (bytes_to_copy > 0) {
        memcpy(dest, src, bytes_to_copy);
    }

    // Clear the newly blanked lines at the bottom of the scroll region
    uint16_t *clear_start = (uint16_t *)VGA_TEXT_BUFFER_ADDR + (priv->state.scroll_lines.end - count) * priv->cols;
    uint16_t blank = ' ' | (priv->state.color << 8);
    for (int i = 0; i < count * priv->cols; i++) {
        clear_start[i] = blank;
    }
}

static void shift_left_char(text_screen_t *s, int row, int col) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    if (row < 0 || row >= priv->rows || col < 0 || col >= priv->cols - 1) {
        return; // Invalid position or cannot shift further left
    }

    uint16_t *dest = get_screen_ptr(s, row, col);
    uint16_t *src = get_screen_ptr(s, row, col + 1);
    size_t bytes_to_copy = (priv->cols - 1 - col) * sizeof(uint16_t);

    if (bytes_to_copy > 0) {
        memcpy(dest, src, bytes_to_copy);
    }
    // Clear the last character in the line
    uint16_t blank = ' ' | (priv->state.color << 8);
    *(get_screen_ptr(s, row, priv->cols - 1)) = blank;
}

static void insert_line(text_screen_t *s, int row) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    if (row < 0 || row >= priv->rows) {
        return; // Invalid row
    }
    
    // Shift content down from the insert row to the end of the scroll region
    uint16_t *dest = (uint16_t *)VGA_TEXT_BUFFER_ADDR + (row + 1) * priv->cols;
    uint16_t *src = (uint16_t *)VGA_TEXT_BUFFER_ADDR + row * priv->cols;
    size_t bytes_to_copy = (priv->state.scroll_lines.end - row - 1) * priv->cols * sizeof(uint16_t);

    if (bytes_to_copy > 0) {
        memmove(dest, src, bytes_to_copy); // Use memmove for overlapping regions
    }

    // Clear the newly inserted blank line
    uint16_t *clear_start = (uint16_t *)VGA_TEXT_BUFFER_ADDR + row * priv->cols;
    uint16_t blank = ' ' | (priv->state.color << 8);
    for (int i = 0; i < priv->cols; i++) {
        clear_start[i] = blank;
    }
}


// text_screen_ops implementations
static void text_screen_putc(text_screen_t *s, char c) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    uint16_t *screen_mem;

    switch (c) {
        case '
': // Newline
            priv->state.pos.col = 0;
            priv->state.pos.row++;
            break;
        case '': // Carriage return
            priv->state.pos.col = 0;
            break;
        case '\b': // Backspace
            if (priv->state.pos.col > 0) {
                priv->state.pos.col--;
                screen_mem = get_screen_ptr(s, priv->state.pos.row, priv->state.pos.col);
                *screen_mem = ' ' | (priv->state.color << 8);
            }
            break;
        default:
            screen_mem = get_screen_ptr(s, priv->state.pos.row, priv->state.pos.col);
            *screen_mem = c | (priv->state.color << 8);
            priv->state.pos.col++;
            break;
    }

    // Handle end of line
    if (priv->state.pos.col >= priv->cols) {
        priv->state.pos.col = 0;
        priv->state.pos.row++;
    }

    // Handle scrolling
    if (priv->state.pos.row >= priv->state.scroll_lines.end) {
        scroll_up(s, 1);
        priv->state.pos.row = priv->state.scroll_lines.end - 1;
    }

    s->ops->update_hardware_cursor(s);
}

static void text_screen_puts(text_screen_t *s, char *str) {
    if (!str) return;
    for (int i = 0; str[i] != '\0'; i++) {
        s->ops->putc(s, str[i]);
    }
}

static void text_screen_putmem(text_screen_t *s, char *mem, int size) {
    if (!mem || size <= 0) return;
    for (int i = 0; i < size; i++) {
        s->ops->putc(s, mem[i]);
    }
}

static void text_screen_clear(text_screen_t *s) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    uint16_t blank = ' ' | (priv->state.color << 8);
    for (int i = 0; i < priv->rows * priv->cols; i++) {
        ((uint16_t *)VGA_TEXT_BUFFER_ADDR)[i] = blank;
    }
    priv->state.pos.row = 0;
    priv->state.pos.col = 0;
    s->ops->update_hardware_cursor(s);
}

static void text_screen_set_pos(text_screen_t *s, int row, int col) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    if (row < 0) row = 0;
    if (row >= priv->rows) row = priv->rows - 1;
    if (col < 0) col = 0;
    if (col >= priv->cols) col = priv->cols - 1;

    priv->state.pos.row = row;
    priv->state.pos.col = col;
    s->ops->update_hardware_cursor(s);
}

static void text_screen_get_pos(text_screen_t *s, int *row, int *col) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    if (row) *row = priv->state.pos.row;
    if (col) *col = priv->state.pos.col;
}

static void text_screen_get_size(text_screen_t *s, int *rows, int *cols) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    if (rows) *rows = priv->rows;
    if (cols) *cols = priv->cols;
}

static void text_screen_set_size(text_screen_t *s, int *rows, int *cols) {
    // For VGA text mode, screen size is fixed to 80x25.
    // This function can be a no-op or return an error/warning for fixed-size screens.
    // For now, we'll just not implement any change.
    // If the underlying hardware supported changing modes, this would be the place.
    // We can update priv->rows and priv->cols but it won't change the actual screen resolution.
    // Leaving it as a no-op for this implementation.
}

static void text_screen_set_text_attr(text_screen_t *s, uint8_t color) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    priv->state.color = color;
}

static void text_screen_get_text_attr(text_screen_t *s, uint8_t *color) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    if (color) *color = priv->state.color;
}

static void text_screen_set_scroll_lines(text_screen_t *s, int begin, int end) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    if (begin < 0) begin = 0;
    if (end > priv->rows) end = priv->rows;
    if (begin >= end) { // Invalid range, reset to full screen
        begin = 0;
        end = priv->rows;
    }
    priv->state.scroll_lines.begin = begin;
    priv->state.scroll_lines.end = end;
}

static void text_screen_get_scroll_lines(text_screen_t *s, int *begin, int *end) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    if (begin) *begin = priv->state.scroll_lines.begin;
    if (end) *end = priv->state.scroll_lines.end;
}

static void text_screen_set_alt_buffer(text_screen_t *s, bool alt) {
    // For now, only one buffer (VGA_TEXT_BUFFER_ADDR) is supported.
    // Implementing alternate buffers would require allocating a second buffer
    // and copying contents between them. This is a placeholder.
    // If 'alt' is true, switch to alt buffer; if false, switch back to main.
    // For this basic implementation, we don't have a true alt buffer.
    // We could potentially copy the current screen to history_buffer if alt is true,
    // and then clear the screen, and restore from history_buffer if alt is false.
    // This is a more advanced feature not covered by direct VGA text mode.
}

static void text_screen_get_alt_buffer(text_screen_t *s, bool *alt) {
    // As per text_screen_set_alt_buffer, no true alt buffer is supported.
    // Always return false for now.
    if (alt) *alt = false;
}


static void text_screen_set_cursor_visible(text_screen_t *s, bool visible) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    priv->state.cursor_visible = visible;
    s->ops->update_hardware_cursor(s);
}

static void text_screen_get_cursor_visible(text_screen_t *s, bool *visible) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    if (visible) *visible = priv->state.cursor_visible;
}

static void text_screen_update_hardware_cursor(text_screen_t *s) {
    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
    uint16_t cursor_location = priv->state.pos.row * priv->cols + priv->state.pos.col;

    if (priv->state.cursor_visible) {
        // Send the high byte of the cursor position
        outb(0x3D4, 14);
        outb(0x3D5, cursor_location >> 8);
        // Send the low byte
        outb(0x3D4, 15);
        outb(0x3D5, cursor_location & 0xFF);

        // Enable cursor (start scanline 14, end scanline 15 for a typical block cursor)
        // This might vary based on the VGA card. Default values are often sufficient.
        outb(0x3D4, 0x0A);
        outb(0x3D5, (inb(0x3D5) & 0xC0) | 14); // Cursor Start Register
        outb(0x3D4, 0x0B);
        outb(0x3D5, (inb(0x3D5) & 0xE0) | 15); // Cursor End Register
    } else {
        // Disable cursor by moving it off-screen or setting start > end
        outb(0x3D4, 0x0A);
        outb(0x3D5, 0x20); // Set bit 5 to disable cursor
    }
}

static void text_screen_destroy(text_screen_t *s) {
    if (s) {
        struct text_screen_priv_data *priv = (struct text_screen_priv_data *)s->priv_data;
        if (priv) {
            // Free history_buffer if it was allocated
            if (priv->history_buffer) {
                kfree(priv->history_buffer);
            }
            kfree(priv);
        }
        kfree(s);
    }
}

// Ops structure definition
static struct text_screen_ops ops = {
    .putc = text_screen_putc,
    .puts = text_screen_puts,
    .putmem = text_screen_putmem,
    .clear = text_screen_clear,
    .set_pos = text_screen_set_pos,
    .get_pos = text_screen_get_pos,
    .get_size = text_screen_get_size,
    .set_size = text_screen_set_size,
    .set_text_attr = text_screen_set_text_attr,
    .get_text_attr = text_screen_get_text_attr,
    .set_scroll_lines = text_screen_set_scroll_lines,
    .get_scroll_lines = text_screen_get_scroll_lines,
    .set_alt_buffer = text_screen_set_alt_buffer,
    .get_alt_buffer = text_screen_get_alt_buffer,
    .set_cursor_visible = text_screen_set_cursor_visible,
    .get_cursor_visible = text_screen_get_cursor_visible,
    .update_hardware_cursor = text_screen_update_hardware_cursor,
    .destroy = text_screen_destroy,
};

// Public creation function
text_screen_t *create_text_screen(int rows, int cols) {
    // For now, we'll ignore `rows` and `cols` and use fixed VGA text mode dimensions
    // as dynamic resizing is not supported by the underlying hardware access.
    // If the user wants specific dimensions, they should specify them in config.h
    // or this function should return NULL if requested dimensions are not 80x25.

    text_screen_t *s = (text_screen_t *)kmalloc(sizeof(text_screen_t)); // Assuming kmalloc is available
    if (!s) return NULL;

    struct text_screen_priv_data *priv = (struct text_screen_priv_data *)kmalloc(sizeof(struct text_screen_priv_data));
    if (!priv) {
        kfree(s); // Assuming kfree is available
        return NULL;
    }

    memset(priv, 0, sizeof(struct text_screen_priv_data));

    priv->rows = DEFAULT_SCREEN_ROWS;
    priv->cols = DEFAULT_SCREEN_COLS;
    priv->state.pos.row = 0;
    priv->state.pos.col = 0;
    priv->state.color = text_attr_of(WHITE, BLACK, false, false); // Default white on black
    priv->state.buffer_num = 0; // Main buffer
    priv->state.scroll_lines.begin = 0;
    priv->state.scroll_lines.end = priv->rows;
    priv->state.cursor_visible = true;

    priv->buffers[0] = (uint16_t *)VGA_TEXT_BUFFER_ADDR;
    priv->buffers[1] = NULL; // Alternate buffer not yet implemented

    s->ops = &ops;
    s->priv_data = priv;

    // Initialize screen and cursor
    s->ops->clear(s);
    s->ops->update_hardware_cursor(s);

    return s;
}

// Color utility functions (from header)
uint8_t text_fg_color(uint8_t color, enum text_color fg, bool bright) {
    return (color & 0xF0) | (fg & 0x0F) | (bright ? 0x08 : 0x00);
}

uint8_t text_bg_color(uint8_t color, enum text_color bg) {
    return (color & 0x0F) | ((bg & 0x07) << 4);
}

uint8_t text_attr_of(enum text_color fg, enum text_color bg, bool bright, bool blink) {
    uint8_t attr = 0;
    attr = (fg & 0x0F);
    if (bright) attr |= 0x08;
    attr |= ((bg & 0x07) << 4);
    if (blink) attr |= 0x80;
    return attr;
}
