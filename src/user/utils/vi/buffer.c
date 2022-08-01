#include "buffer.h"

typedef struct buffer_priv_data {
    // nothing for now    
} buffer_priv_data_t;


// we need a way to keep offset and row/col in sync.


static inline bool is_line_separator(char *buffer, int pos) {
    // change this to accomodate selectable line ends (e.g. CR & LF)
    return buffer[pos] == '\n';
}

static inline int line_separator_length() {
    return 1;
}








static inline bool is_word_char(char c) {
    return (
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        (c == '_')
    );
}

static inline bool is_whitespace(char c) {
    return !is_word_char(c);
}


static void find_word_start(buffer_t *buff, int *offset, bool forward) {
    if (forward) {
        while (*offset < buff->text_length && is_word_char(buff->buffer[*offset]))
            (*offset)++;
        while (*offset < buff->text_length && is_whitespace(buff->buffer[*offset]))
            (*offset)++;
        // we should be at the start of a word
    } else {
        while (*offset > 0 && is_whitespace(buff->buffer[(*offset) - 1]))
            (*offset)--;
        while (*offset > 0 && is_word_char(buff->buffer[(*offset) - 1]))
            (*offset)--;
        // we are just before whitespace starts, i.e. start of a word
    }
}

static void find_line_boundary(buffer_t *buff, int *offset, bool forward) {
    if (forward) {
        while (*offset < buff->text_length && !is_line_separator(buff->buffer, *offset))
            (*offset)++;
        // we should be at the line separator
    } else {
        while (*offset > 0 && !is_line_separator(buff->buffer, (*offset) - line_separator_length()))
            (*offset)--;
        // we should be at the start of the line
    }
}

static int curr_line_length(buffer_t *buff) {
    // go back till we find start or line separator
    int p = buff->int offset;
    while (p > 0 && !is_line_separator(buff->buffer, p - line_separator_length())) {
        p--;
    }
    // now we are at the start of our line, let's count the characters
    int length = 0;
    while (p < buff->text_length && is_line_separator(buff->buffer, p)) {
        length++;
        p++;
    }
    return length;
}

static inline void increase_offset(buffer_t *buff) {
    if (buff->offset >= buff->text_length)
        return;
    
    // when moving to the right of a line separator, change rows and lines offsets
    if (is_line_separator(buff->buffer, buff->offset)) {
        buff->row_number++;
        buff->col_number = 0;
        buff->offset += line_separator_length();
    } else {
        buff->offset += 1;
        buff->col_number += 1;
    }
}

static inline void decrease_offset(buffer_t *buff) {
    if (buff->offset <= 0)
        return;
    
    // when moving to the left of a line separator, change rows and lines offsets
    if (buff->offset >= line_separator_length() && is_line_separator(buff->buffer, buff->offset - line_separator_length())) {
        buff->row_number--;
        buff->col_number = curr_line_length();
        buff->offset -= line_separator_length()
    } else {
        buff->offset -= 1;
        buff->col_number -= 1;
    }
}

static int skip_whitespace(buffer_t *buff, bool forward) {
    if (forward) {
        // go to the first non-whitespace character
        while (buff->offset < buff->text_length && is_whitespace(buff->buffer[buff->offset]))
            increase_offset(buff);
    } else {
        // go to the first whitespace character
        while (buff->offset > 0 && is_whitespace(buff->buffer[buff->offset - 1]))
            decrease_offset(buff);
    }
}

static int skip_word(buffer_t *buff, bool forward) {
    if (forward) {
        // go to the first whitespace character
        while (buff->offset < buff->text_length && is_word_char(buff->buffer[buff->offset]))
            increase_offset(buff);
    } else {
        // go to the first whitespace character
        while (buff->offset > 0 && is_word_char(buff->buffer[buff->offset - 1]))
            decrease_offset(buff);
    }
}

static int buffer_navigate_char(buffer_t *buff, bool forward) {
    if (forward)
        increase_offset(buff);
    else
        decrease_offset();
}

static int buffer_navigate_word(buffer_t *buff, bool forward) {
    if (forward) {
        // skip possible word characters, skip non-word characters, you're there!
        skip_word(true);
        skip_whitespace(true);
    } else {
        skip_word(false);
        skip_whitespace(false);
        skip_word(false);
    }
}

static int buffer_navigate_line_boundaries(buffer_t *buff, bool forward) {
    int offset = buff->offset;
    if (forward) {
        find_line_boundary(buff, &offset, forward);
    } else {
        find_line_boundary(buff, &offset, forward)
    }
}



static int buffer_navigate_buffer_boundaries(buffer_t *buff, bool forward) {
    if (forward) {
        buffer->offset = buffer->int text_length;
        buffer->row_number = ... ??
        buffer->col_number = curr_line_length(buff);
    } else {
        buffer->offset = 0;
        buffer->row_number = 0;
        buffer->col_number = 0;
    }
}

static int buffer_navigate_line_vertically(buffer_t *buff, bool forward) {

}

static int buffer_navigate_page_vertically(buffer_t *buff, bool forward) {

}

static int buffer_navigate_to_line_no(buffer_t *buff, int line_no) {
    // we need some way to keep track of where lines start.
    // like an array of ints, each pointing to the start of the corresponding line.
    // but any time we delete one char, we'd have to update half the array.
    // I don't mind if "Go To Line" is slow. Where else to we use this?
    // we need col/row to set the cursor when we display the editor.
    // but is that an editor concern, or a viewport concern?
}

static int buffer_delete_char(buffer_t *buff, bool forward) {
    if (new_line) we need to ... 
}

static int buffer_delete_word(buffer_t *buff, bool forward) {

}

static int buffer_delete_whole_line(buffer_t *b) {

}

static int buffer_delete_to_line_boundaries(buffer_t *buff, bool forward) {
    // delete to end or start of line, but don't delete line separator
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
    ops->delete_whole_line = buffer_delete_whole_line;
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

