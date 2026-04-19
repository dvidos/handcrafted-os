#include "../include/uapi/base.h"

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

    } state;

    uint16_t *buffers[2];
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

    void set_color(text_screen_t *s, uint8_t color);
    void get_color(text_screen_t *s, uint8_t *color);

    void set_scroll_lines(text_screen_t *s, int begin, int end);
    void get_scroll_lines(text_screen_t *s, int *begin, int *end);

    void set_alt_buffer(bool alt);
    void get_alt_buffer(bool *alt);

    void destroy(text_screen_t *s);
};

text_screen_t *create_text_screen(int row, int cols);
