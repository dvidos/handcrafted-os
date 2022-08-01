#ifndef _BUFFER_H
#define _BUFFER_H

typedef struct buffer_ops buffer_ops_t;
typedef struct buffer_priv_data buffer_priv_data_t;

typedef struct buffer {
    char *buffer;
    int buffer_size;
    int row_number;
    int col_number;
    int offset;
    int text_length;

    buffer_ops_t *ops;
    buffer_priv_data_t *priv;
} buffer_t;

struct buffer_ops {
    int (*navigate_char)(buffer_t *buff, bool forward);
    int (*navigate_word)(buffer_t *buff, bool forward);
    int (*navigate_line_boundaries)(buffer_t *buff, bool forward);
    int (*navigate_buffer_boundaries)(buffer_t *buff, bool forward);
    int (*navigate_line_vertically)(buffer_t *buff, bool forward);
    int (*navigate_page_vertically)(buffer_t *buff, bool forward);
    int (*navigate_to_line_no)(buffer_t *buff, int line_no);

    int (*delete_char)(buffer_t *buff, bool forward);
    int (*delete_word)(buffer_t *buff, bool forward);
    int (*delete_whole_line)(buffer_t *b);
    int (*delete_to_line_boundaries)(buffer_t *buff, bool forward);

    int (*insert_char)(buffer_t *buff, int chr);
    int (*insert_str)(buffer_t *buff, char *text);
};

buffer *create_buffer(int buffer_size);
void destroy_buffer(buffer_t *buff);


#endif
