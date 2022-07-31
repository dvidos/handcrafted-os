#include "buffer.h"

typedef struct buffer_priv_data {
    // nothing for now    
} buffer_priv_data_t;


static int buffer_navigate_char(buffer_t *buff, bool forward) {

}

static int buffer_navigate_word(buffer_t *buff, bool forward) {

}

static int buffer_navigate_line_boundaries(buffer_t *buff, bool forward) {

}

static int buffer_navigate_buffer_boundaries(buffer_t *buff, bool forward) {

}

static int buffer_navigate_line_vertically(buffer_t *buff, bool forward) {

}

static int buffer_navigate_page_vertically(buffer_t *buff, bool forward) {

}

static int buffer_navigate_to_line_no(buffer_t *buff, int line_no) {

}

static int buffer_delete_char(buffer_t *buff, bool forward) {

}

static int buffer_delete_word(buffer_t *buff, bool forward) {

}

static int buffer_delete_while_line(buffer_t *b) {

}

static int buffer_delete_to_line_boundaries(buffer_t *buff, bool forward) {

}

static int buffer_insert_char(buffer_t *buff, int chr) {

}

static int buffer_insert_str(buffer_t *buff, char *text)  {

}


buffer_t *create_buffer(int buffer_size) {
    buffer_t *buffer = malloc(sizeof(buffer_t));

    buffer_priv_data_t *priv_data = malloc(sizeof(buffer_priv_data_t));
    buffer->priv_data = priv_data;

    buffer_ops_t *ops = malloc(sizeof(buffer_ops_t));
    ops->navigate_char = buffer_navigate_char;
    ops->navigate_word = buffer_navigate_word;
    ops->navigate_line_boundaries = buffer_navigate_line_boundaries;
    ops->navigate_buffer_boundaries = buffer_navigate_buffer_boundaries;
    ops->navigate_line_vertically = buffer_navigate_line_vertically;
    ops->navigate_page_vertically = buffer_navigate_page_vertically;
    ops->navigate_to_line_no = buffer_navigate_to_line_no;
    ops->delete_char = buffer_delete_char;
    ops->delete_word = buffer_delete_word;
    ops->delete_while_line = buffer_delete_while_line;
    ops->delete_to_line_boundaries = buffer_delete_to_line_boundaries;
    ops->insert_char = buffer_insert_char;
    ops->insert_str = buffer_insert_str;
    buffer->ops = ops;

    return buffer;
}

void destroy_buffer(buffer_t *buff) {
    if (buff->ops)
        free(buff->ops);
    if (buff->priv_data)
        free(buff->priv_data);
    free(buff);
}

