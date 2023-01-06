#ifndef _BUFFER_H
#define _BUFFER_H

#include <ctypes.h>
#include "line.h"


typedef struct buffer_ops buffer_ops_t;

typedef struct buffer {
    /* our storage will be an array of pointers.
       this helps with faster deleting or inserting lines
       each pointer will point to a memory allocated buffer, of size and double size.

       in theory, we'll always have at least one line, even for an empty file.
       when saving the lines, the last line shall not have line separator.
    */
    line_t *lines[16 * 1024]; // 16K lines limit for now
    int lines_count;
    int row_number; // zero based
    int col_number;
    bool modified;

    buffer_ops_t *ops;
} buffer_t;

struct buffer_ops {
    void (*navigate_char)(buffer_t *buff, bool forward);
    void (*navigate_word)(buffer_t *buff, bool forward);
    void (*navigate_line_boundaries)(buffer_t *buff, bool forward);
    void (*navigate_buffer_boundaries)(buffer_t *buff, bool forward);
    void (*navigate_line_vertically)(buffer_t *buff, bool forward);
    void (*navigate_page_vertically)(buffer_t *buff, int lines, bool forward);
    void (*navigate_to_line_no)(buffer_t *buff, int line_no);
  
    void (*delete_char)(buffer_t *buff, bool forward);
    void (*delete_word)(buffer_t *buff, bool forward);
    void (*delete_whole_line)(buffer_t *b);
    void (*delete_to_line_boundaries)(buffer_t *buff, bool forward);
    void (*insert_char)(buffer_t *buff, int chr);
    void (*insert_str)(buffer_t *buff, char *text);

    void (*join_line)(buffer_t *buff, bool from_below);
    void (*split_line)(buffer_t *buff);
};

buffer_t *create_buffer();
void destroy_buffer(buffer_t *buff);


#endif
