#pragma once

#include "../include/uapi/base.h"
#include "../include/uapi/key_event.h"
#include "../../utils/mutex.h"
#include "../include/ctypes.h"








enum text_color { BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, YELLOW, WHITE };


typedef struct vconsole vconsole_t;
typedef void (console_buffer_modified_func)(vconsole_t *vc);


struct vconsole {
    struct vconsole_ops *ops;
    struct vconsole *next;
    void *data;
};

typedef enum vconsole_flag {
    CANONICAL_MODE   = (1 << 0), // local: enables line buffering, blocks till '\n', supports backspace, Cltr+D=eof, etc. off = keys returned immediately
    ECHO             = (1 << 1), // local: enables echoing of the keys that are being handled / returned
    SIGNAL_HANDLING  = (1 << 2), // local: makes Ctrl+C behave like a kill signal, instead of being returned
    CR_TO_LF         = (1 << 3), // input: makes '\r' (enter) send '\n' (new line)
    FLOW_CONTROL     = (1 << 4), // input: allows Ctrl+S/Ctrl+Q to stop/continue draining the buffer
    LF_TO_CRLF       = (1 << 5)  // output: makes '\n' not only go one down, but also back to col zero
} vconsole_flag_t;


struct vconsole_ops {

    void (*clear)(vconsole_t *vc);
    void (*putc)(vconsole_t *vc, char c);
    void (*puts)(vconsole_t *vc, const char *str);
    
    // used primarily by the tty_fops FS driver
    int  (*read)(vconsole_t *vc, char *buff, int size);
    void (*write)(vconsole_t *vc, const char *buff, int size);
    
    void (*set_pos)(vconsole_t *vc, int row, int col);
    void (*get_pos)(vconsole_t *vc, int *row, int *col);
    void (*get_size)(vconsole_t *vc, int *rows, int *cols);
    void (*set_size)(vconsole_t *vc, int rows, int cols);
    void (*set_text_attr)(vconsole_t *vc, uint8_t color);
    void (*get_text_attr)(vconsole_t *vc, uint8_t *color);
    void (*set_scroll_lines)(vconsole_t *vc, int begin, int end);
    void (*get_scroll_lines)(vconsole_t *vc, int *begin, int *end);
    void (*set_alt_buffer)(vconsole_t *vc, bool alt);
    void (*get_alt_buffer)(vconsole_t *vc, bool *alt);
    void (*set_cursor_visible)(vconsole_t *vc, bool visible);
    void (*get_cursor_visible)(vconsole_t *vc, bool *visible);
    void (*get_buffer_address)(vconsole_t *vc, void **address);

    void (*set_title)(vconsole_t *vc, char *title);
    const char *(*get_title)(vconsole_t *vc);
    
    // these usually used by ioctl
    void (*set_flag)(vconsole_t *vc, vconsole_flag_t flag, bool enabled);
    bool (*get_flag)(vconsole_t *vc, vconsole_flag_t flag);
    
    void (*enqueue_key)(vconsole_t *vc, key_event_t *key);
    void (*destroy)(vconsole_t *vc);
    
    // History getters for console manager
    uint16_t *(*get_history_line)(vconsole_t *vc, int index);
    int (*get_history_count)(vconsole_t *vc);
    int (*get_view_offset)(vconsole_t *vc);
    void (*set_view_offset)(vconsole_t *vc, int offset);
};

vconsole_t *create_vconsole(int rows, int cols, console_buffer_modified_func *on_modified);

uint8_t vconsole_fg_color(uint8_t color, enum text_color fg, bool bright);
uint8_t vconsole_bg_color(uint8_t color, enum text_color bg);
uint8_t vconsole_color(enum text_color fg, enum text_color bg, bool bright, bool blink);






