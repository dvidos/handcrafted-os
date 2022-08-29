#include "buffer.h"
#include "ctypes.h"

typedef struct buffer_priv_data {
    // nothing for now    
} buffer_priv_data_t;



int line_separator_length();
bool is_line_separator(char *ptr);
bool is_word_char(char c);
int offset_to_line(char *buffer, int text_length, int offset);
int line_to_offset(char *buffer, int text_length, int line);
int count_line_length(char *buffer, int text_length, int line_start_offset);
int count_buffer_lines(char *buffer, int text_length);
int find_start_of_word(char *buffer, int text_length, bool right, int offset);
int find_start_of_line(char *buffer, int text_length, bool right, int offset);



// fast inline information
inline int line_separator_length() {
	return 1;
}

// fast inline decision maker
inline bool is_line_separator(char *ptr) {
	return *ptr == '\n';
}

// fast inline decision maker
inline bool is_word_char(char c) {
	return (
		(c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9') ||
		(c == '_')
	);
}

// find the line number in given offset
int offset_to_line(char *buffer, int text_length, int offset) {
	int line = 0;
	
	// a running pointer should be the faster way
	char *p = buffer;
    char *past_end = buffer + text_length;

	while (p < past_end && offset > 0) {
		if (is_line_separator(p))
			line++;
		p++;
		offset--;
	}
	
	return line;
}

// find the start of the line as offset
int line_to_offset(char *buffer, int text_length, int line) {
	int offset = 0;
	
	// a running pointer should be the faster way
	char *p = buffer;
    char *past_end = buffer + text_length;

	while (p < past_end && line > 0) {
		if (is_line_separator(p))
			line--;
		p++;
		offset++;
	}
	
	return offset;
}

// does not include line separators
int count_line_length(char *buffer, int text_length, int line_start_offset) {
	int length = 0;
	
	// a running pointer should be the faster way
	char *p = buffer + line_start_offset;
    char *past_end = buffer + text_length;

	while (p < past_end && !is_line_separator(p)) {
		p++;
		length++;
	}
	
	return length;
}

int count_buffer_lines(char *buffer, int text_length) {
	int lines = 1; // even without a single line separator
	char *p = buffer;
	char *end = buffer + text_length;

	while (p < end) {
		if (is_line_separator(p))
			lines++;
		p++;
	}
	
	return lines;
}

int find_start_of_word(char *buffer, int text_length, bool right, int offset) {
    char *p = buffer + offset;
    char *past_end = buffer + text_length;

    if (right) {
        // skip word
        while (p < past_end && is_word_char(*p))
            p++;
        // skip whitespace
        while (p < past_end && !is_word_char(*p))
            p++;
    } else {
        // skip whitespace
        while (p > buffer && !is_word_char(*(p - 1)))
            p--;
        // skip word
        while (p > buffer && is_word_char(*(p - 1)))
            p--;
    }

    return (p - buffer);
}

int find_start_of_line(char *buffer, int text_length, bool right, int offset) {
    char *p = buffer + offset;
    char *past_end = buffer + text_length;

    if (right) {
        while (p < past_end && !is_line_separator(p))
            p++;
        if (p < past_end && is_line_separator(p))
            p += line_separator_length();
    } else {
        if (p > buffer && is_line_separator(p - line_separator_length()))
            p -= line_separator_length();
        while (p > buffer && !is_line_separator(p - line_separator_length()))
            p--;
    }

    return (p - buffer);
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
        find_line_boundary(buff, &offset, forward);
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
    if (buff->priv_data)
        free(buff->priv_data);
    if (buff->ops)
        free(buff->ops);
    free(buff);
}

