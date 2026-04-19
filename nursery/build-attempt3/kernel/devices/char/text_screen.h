#include "../include/uapi/base.h"

enum text_color { BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, YELLOW, WHITE };


typedef struct text_screen text_screen_t;

struct text_screen {
    struct text_screen_ops *ops;
    void *priv_data;
};

struct text_screen_priv_data {
    int rows;
    int cols;

    struct state {
        struct pos {
            int row;
            int col;
        } pos;
        uint8_t color;
        uint8_t buffer_num;
        struct scroll_lines {
            int begin;
            int end; // exclusive
        } scroll_lines;
        bool cursor_visible;

    } state;

    uint16_t *history_buffer;
    int history_max_rows;
    int history_count;
    int history_head;

    uint16_t *buffers[2];
    int view_offset;
    bool dirty;
};

struct text_screen_ops {
    void putc(text_screen_t *s, char c);
    void puts(text_screen_t *s, char *str);
    void putmem(text_screen_t *s, char *mem, int size);
    
    void clear(text_screen_t *s);
    
    void set_pos(text_screen_t *s, int row, int col);
    void get_pos(text_screen_t *s, int *row, int *col);

    void get_size(text_screen_t *s, int *rows, int *cols);
    void set_size(text_screen_t *s, int *rows, int *cols);

    void set_text_attr(text_screen_t *s, uint8_t color);
    void get_text_attr(text_screen_t *s, uint8_t *color);

    void set_scroll_lines(text_screen_t *s, int begin, int end);
    void get_scroll_lines(text_screen_t *s, int *begin, int *end);

    void set_alt_buffer(text_screen_t *s, bool alt);
    void get_alt_buffer(text_screen_t *s, bool *alt);

    void set_cursor_visible(text_screen_t *s, bool visible);
    void get_cursor_visible(text_screen_t *s, bool *visible);

    void update_hardware_cursor(text_screen_t *s); // outport() to VGA registers

    void destroy(text_screen_t *s);
};

text_screen_t *create_text_screen(int row, int cols);

uint8_t text_fg_color(uint8_t color, enum text_color fg, bool bright);
uint8_t text_bg_color(uint8_t color, enum text_color bg);
uint8_t text_attr_of(enum text_color fg, enum text_color bg, bool bright, bool blink);
