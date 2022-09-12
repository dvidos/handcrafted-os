#include <ctypes.h>
#include <stdlib.h>
#include <string.h>
#include "line.h"

#define WORD_CHARS   "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_"


static bool at_start_of_line(line_t *line, int column) {
    return column == 0;
}

static bool at_end_of_line(line_t *line, int column) {
    return column == line->length;
}

static inline bool is_word_char(char c) {
    return 
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'A') ||
        (c >= '0' && c <= '9') ||
        (c == '_');
}

static void navigate_word(line_t *line, int *column, bool forward) {
    if (forward) {
        while (*column < line->length && is_word_char(line->buffer[*column]))
            *column += 1;
        while (*column < line->length && !is_word_char(line->buffer[*column]))
            *column += 1;
    } else {
        while (*column > 0 && !is_word_char(line->buffer[*column - 1]))
            *column -= 1;
        while (*column > 0 && is_word_char(line->buffer[*column - 1]))
            *column -= 1;
    }
}


static void ensure_adequate_buffer_size(line_t *line, int size_needed) {
    if (line->length + 1 + size_needed <= line->allocated)
        return;

    int new_size = line->allocated * 2;
    while (line->length + 1 + size_needed > new_size)
        new_size *= 2;
    
    char *p = malloc(new_size);
    memcpy(p, line->buffer, line->length + 1);
    free(line->buffer);
    line->buffer = p;
    line->allocated = new_size;
}

static void insert_char(line_t *line, int *column, int chr) {
    ensure_adequate_buffer_size(line, 1);

    // make room as needed
    memmove(line->buffer + *column + 1, line->buffer + *column, line->length - *column + 1);
    line->buffer[*column] = chr;
    line->length += 1;
    line->buffer[line->length] = 0;
    *column += 1;
}

static void insert_str(line_t *line, int *column, char *text) {
    int text_len = strlen(text);
    ensure_adequate_buffer_size(line, text_len);

    // make room as needed
    memmove(line->buffer + *column + text_len, line->buffer + *column, line->length - *column + 1);
    memcpy(line->buffer + *column, text, text_len);
    line->length += text_len;
    line->buffer[line->length] = 0;
    *column += text_len;
}

static void delete_char(line_t *line, int *column, bool forward) {
    if (forward) {
        if (*column < line->length) {
            strcpy(line->buffer + *column, line->buffer + *column + 1);
            line->length--;
        }
    } else {
        if (*column > 0) {
            strcpy(line->buffer + *column - 1, line->buffer + *column);
            line->length--;
            *column -= 1;
        }
    }
}

static void delete_word(line_t *line, int *column, bool forward) {
    // find where we'd navigate
    int new_column = *column;
    navigate_word(line, &new_column, forward);

    if (forward) {
        int len = new_column - *column;
        strcpy(line->buffer + *column, line->buffer + new_column);
        line->length -= len;
    } else {
        int len = *column - new_column;
        strcpy(line->buffer + new_column, line->buffer + *column);
        line->length -= len;
        *column -= len;
    }
}

static void delete_to_line_boundaries(line_t *line, int *column, bool forward) {
    if (forward) {
        line->length = *column;
        line->buffer[line->length] = 0;
    } else {
        strcpy(line->buffer, line->buffer + *column);
        line->length -= *column;
        *column = 0;
    }
}

static void clear_line(line_t *line, int *column) {
    line->length = 0;
    line->buffer[0] = 0;
    *column = 0;
}

line_t *create_line() {

    line_ops_t *ops = malloc(sizeof(line_ops_t));

    ops->at_start_of_line = at_start_of_line;
    ops->at_end_of_line = at_end_of_line;
    ops->navigate_word = navigate_word;
    ops->insert_char = insert_char;
    ops->insert_str = insert_str;
    ops->delete_char = delete_char;
    ops->delete_word = delete_word;
    ops->delete_to_line_boundaries = delete_to_line_boundaries;
    ops->clear_line = clear_line;

    line_t *line = malloc(sizeof(line_t));

    // allocated size must never be zero
    // it must hold at least the zero terminator character

    int initial_size = 8;
    line->buffer = malloc(initial_size);
    line->allocated = initial_size;
    line->length = 0;
    line->buffer[0] = '\0';
    line->ops = ops;

    return line;
}

void destroy_line(line_t *line) {
    if (line->buffer)
        free(line->buffer);
    if (line->ops)
        free(line->ops);
    free(line);
}
