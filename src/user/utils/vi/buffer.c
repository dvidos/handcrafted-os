#include "buffer.h"

typedef struct buffer_priv_data {
    // nothing for now    
} buffer_priv_data_t;





// fast inline information
static inline int line_separator_length() {
	return 1;
}

// fast inline decision maker
static inline bool is_line_separator(char *ptr) {
	return *ptr == '\n';
}

// fast inline decision maker
static inline bool is_word_char(char c) {
	return (
		(c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9') ||
		(c == '_')
	);
}

// find the line number in given offset
static int offset_to_line(char *buffer, int offset) {
	int line = 0;
	
	// a running pointer should be the faster way
	char *p = buffer;
	while (offset > 0) {
		if (is_line_separator(p))
			line++;
		p++;
		offset--;
	}
	
	return line;
}

// find the start of the line as offset
static int line_to_offset(char *buffer, int line) {
	int offset = 0;
	
	// a running pointer should be the faster way
	char *p = buffer;
	while (line > 0) {
		if (is_line_separator(p))
			line--;
		p++;
		offset++;
	}
	
	return offset;
}

static int count_line_length(char *buffer, int line_start_offset) {
	int length = 0;
	
	// a running pointer should be the faster way
	char *p = buffer + line_start_offset;
	while (!is_line_separator(p)) {
		p++;
		length++;
	}
	
	return length;
}

static int count_buffer_lines(char *buffer, int text_length) {
	int lines = 0;
	
	char *p = buffer;
	char *end = buffer + text_length;
	while (p <= end) {
		if (is_line_separator(p))
			lines++;
		p++;
	}
	
	return lines;
}

static int find_start_of_word(bool right, int *offset) {
	if (right) {
		// skip possible current word
		// skip any non-word
	} else {
		// skip possible non-word to the left
		// find start of word left
	}
}

static int find_line_start(int *offset) {
}

static int find_line_end(int *offset) {
}





// ---------------------------------------------------------------------------




static int buffer_navigate_char(buffer_t *buff, bool forward) {
    if (forward)
        ;
    else
        ;
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

