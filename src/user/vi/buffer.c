#include <ctypes.h>
#include <string.h>
#include <stdlib.h>
#include "buffer.h"
#include "line.h"


static void _create_and_insert_line(buffer_t *buff, int index) {
    memmove(
        buff->lines + index + 1,
        buff->lines + index,
        buff->lines_count - index
    );
    line_t *new_line = create_line();
    buff->lines[index] = new_line;
    buff->lines_count++;
    buff->modified = true;
}

static void _remove_and_destroy_line(buffer_t *buff, int line_no) {
    line_t *curr_line = buff->lines[line_no];
    destroy_line(curr_line);
    buff->lines[line_no] = NULL;
    memmove(
        buff->lines + line_no,
        buff->lines + line_no + 1,
        buff->lines_count - line_no - 1
    );
    buff->lines_count--;
    buff->modified = true;
}

static void _ensure_row_number_within_bounds(buffer_t *buff) {
    if (buff->row_number > buff->lines_count - 1)
        buff->row_number = buff->lines_count - 1;
    if (buff->row_number < 0)
        buff->row_number = 0;
}

static void _ensure_col_number_within_bounds(buffer_t *buff) {
    line_t *line = buff->lines[buff->row_number];
    if (buff->col_number > line->length)
        buff->col_number = line->length;
    if (buff->col_number < 0)
        buff->col_number = 0;
}

static void navigate_char(buffer_t *buff, bool forward) {
    line_t *curr_line = buff->lines[buff->row_number];
    if (forward) {
        if (curr_line->ops->at_end_of_line(curr_line, buff->col_number)) {
            // go to start of next line
            if (buff->row_number < buff->lines_count - 1) {
                buff->row_number += 1;
                buff->col_number = 0;
            }
        } else {
            buff->col_number += 1;
        }
    } else {
        if (curr_line->ops->at_start_of_line(curr_line, buff->col_number)) {
            // go to end of prev line
            if (buff->row_number > 0) {
                buff->row_number -= 1;
                curr_line = buff->lines[buff->row_number];
                buff->col_number = curr_line->length;
            }
        } else {
            buff->col_number -= 1;
        }
    }
}

static void navigate_word(buffer_t *buff, bool forward) {
    line_t *curr_line = buff->lines[buff->row_number];
    
    if (forward) {
        if (curr_line->ops->at_end_of_line(curr_line, buff->col_number)) {
            // move to start of next line
            if (buff->row_number < buff->lines_count - 1) {
                buff->row_number += 1;
                curr_line = buff->lines[buff->row_number];
                buff->col_number = 0;
                curr_line->ops->navigate_word(curr_line, &buff->col_number, forward);
            }
        } else {
            curr_line->ops->navigate_word(curr_line, &buff->col_number, forward);
        }
    } else {
        if (curr_line->ops->at_start_of_line(curr_line, buff->col_number)) {
            // move to end of prev line
            if (buff->row_number > 0) {
                buff->row_number -= 1;
                curr_line = buff->lines[buff->row_number];
                buff->col_number = curr_line->length;
                curr_line->ops->navigate_word(curr_line, &buff->col_number, forward);
            }
        } else {
            curr_line->ops->navigate_word(curr_line, &buff->col_number, forward);
        }
    }
}

static void navigate_line_boundaries(buffer_t *buff, bool forward) {
    line_t *curr_line = buff->lines[buff->row_number];
    curr_line->ops->navigate_line_boundaries(curr_line, &buff->col_number, forward);
}


static void navigate_buffer_boundaries(buffer_t *buff, bool forward) {
    if (forward) {
        buff->row_number = buff->lines_count - 1;
        buff->col_number = buff->lines[buff->row_number]->length;
    } else {
        buff->row_number = 0;
        buff->col_number = 0;
    }
}

static void navigate_line_vertically(buffer_t *buff, bool forward) {
    if (forward) {
        if (buff->row_number < buff->lines_count - 1) {
            buff->row_number += 1;
        }
    } else {
        if (buff->row_number > 0) {
            buff->row_number -= 1;
        }
    }
    _ensure_col_number_within_bounds(buff);
}

static void navigate_page_vertically(buffer_t *buff, int lines, bool forward) {
    if (forward) {
        buff->row_number += lines;
    } else {
        buff->row_number -= lines;
    }
    _ensure_row_number_within_bounds(buff);
    _ensure_col_number_within_bounds(buff);
}

static void navigate_to_line_no(buffer_t *buff, int line_no) {
    buff->row_number = line_no;
    _ensure_row_number_within_bounds(buff);
    _ensure_col_number_within_bounds(buff);
}

static void delete_char(buffer_t *buff, bool forward) {
    line_t *curr_line = buff->lines[buff->row_number];
    if (!curr_line->ops->at_end_of_line(curr_line, buff->col_number)) {
        curr_line->ops->delete_char(curr_line, &buff->col_number, forward);
        buff->modified = true;
    }
}

static void buffer_delete_word(buffer_t *buff, bool forward) {
    line_t *curr_line = buff->lines[buff->row_number];
    if (!curr_line->ops->at_end_of_line(curr_line, buff->col_number)) {
        curr_line->ops->delete_word(curr_line, &buff->col_number, forward);
        buff->modified = true;
    }
}

// remove the line altogether, destroy it as well
static void delete_whole_line(buffer_t *buff) {
    if (buff->lines_count == 1) {
        // we must always have at least one line structure
        line_t *curr_line = buff->lines[buff->row_number];
        curr_line->ops->clear_line(curr_line, &buff->col_number);
        return;
    }
    
    _remove_and_destroy_line(buff, buff->row_number);

    // if we erased last one, we'll need to move up
    _ensure_row_number_within_bounds(buff);
    _ensure_col_number_within_bounds(buff);
}

static void delete_to_line_boundaries(buffer_t *buff, bool forward) {
    // delete to end or start of line, but don't delete line separator
    line_t *curr_line = buff->lines[buff->row_number];
    curr_line->ops->delete_to_line_boundaries(curr_line, &buff->col_number, forward);
    buff->modified = true;
}

static void insert_char(buffer_t *buff, int chr) {
    line_t *curr_line = buff->lines[buff->row_number];
    curr_line->ops->insert_char(curr_line, &buff->col_number, chr);
    buff->modified = true;
}

static void buffer_insert_str(buffer_t *buff, char *text)  {
    line_t *curr_line = buff->lines[buff->row_number];
    curr_line->ops->insert_str(curr_line, &buff->col_number, text);
    buff->modified = true;
}

static void join_line(buffer_t *buff, bool from_below) {
    line_t *curr_line = buff->lines[buff->row_number];
    if (from_below) {
        // we join line below to the current one
        if (buff->row_number < buff->lines_count - 1) {
            line_t *below_line = buff->lines[buff->row_number - 1];

            // go to end of line, append line below, remove line below
            buff->col_number = curr_line->length;
            // we don't want to move the current col.
            int temp_col = buff->col_number;
            curr_line->ops->insert_str(curr_line, &temp_col, below_line->buffer);
            buff->modified = true;

            // now we want to remove the line below
            _remove_and_destroy_line(buff, buff->row_number + 1);
        }
    } else {
        // we join current to the line above
        if (buff->row_number > 0) {
            line_t *above_line = buff->lines[buff->row_number - 1];

            // at the line above, find end of line, append line below as in previous case
            buff->col_number = above_line->length;
            int temp_col = buff->col_number;
            above_line->ops->insert_str(above_line, &temp_col, curr_line->buffer);
            buff->modified = true;

            // now we remove what used to be the old line
            buff->row_number -= 1;
            _remove_and_destroy_line(buff, buff->row_number + 1);
        }
    }
}

static void split_line(buffer_t *buff) {
    // split at curent point, insert what's left in line below, go to start of line below
    line_t *curr_line = buff->lines[buff->row_number];

    _create_and_insert_line(buff, buff->row_number + 1);
    line_t *line_below = buff->lines[buff->row_number + 1];

    // move everything after the curr column to the line below
    int temp_col = 0;
    line_below->ops->insert_str(line_below, &temp_col, curr_line->buffer + buff->col_number);
    curr_line->ops->delete_to_line_boundaries(curr_line, &buff->col_number, true);
    buff->modified = true;

    buff->row_number += 1;
    buff->col_number = 0;
}

buffer_t *create_buffer() {
    buffer_t *buff = malloc(sizeof(buffer_t));
    memset(buff, 0, sizeof(buffer_t));

    // a buffer always has at least one line.
    _create_and_insert_line(buff, 0);
    buff->modified = false;

    buffer_ops_t *ops = malloc(sizeof(buffer_ops_t));
    ops->navigate_char = navigate_char;
    ops->navigate_word = navigate_word;
    ops->navigate_line_boundaries = navigate_line_boundaries;
    ops->navigate_buffer_boundaries = navigate_buffer_boundaries;
    ops->navigate_line_vertically = navigate_line_vertically;
    ops->navigate_page_vertically = navigate_page_vertically;
    ops->navigate_to_line_no = navigate_to_line_no;
    ops->delete_char = delete_char;
    ops->delete_word = buffer_delete_word;
    ops->delete_whole_line = delete_whole_line;
    ops->delete_to_line_boundaries = delete_to_line_boundaries;
    ops->insert_char = insert_char;
    ops->insert_str = buffer_insert_str;
    ops->join_line = join_line;
    ops->split_line = split_line;

    buff->ops = ops;

    return buff;
}

void destroy_buffer(buffer_t *buff) {
    if (buff->ops)
        free(buff->ops);
    free(buff);
}

