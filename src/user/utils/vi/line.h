#ifndef _LINE_H
#define _LINE_H

#include <ctypes.h>


typedef struct line_ops line_ops_t;

typedef struct line {
    char *buffer;
    int allocated;
    int length;
    line_ops_t *ops;
} line_t;

struct line_ops {
    bool (*at_start_of_line)(line_t *line, int column);
    bool (*at_end_of_line)(line_t *line, int column);

    void (*navigate_word)(line_t *line, int *column, bool forward);
    
    void (*insert_char)(line_t *line, int *column, int chr);
    void (*insert_str)(line_t *line, int *column, char *text);

    void (*delete_char)(line_t *line, int *column, bool forward);
    void (*delete_word)(line_t *line, int *column, bool forward);
    void (*delete_to_line_boundaries)(line_t *line, int *column, bool forward);

    void (*clear_line)(line_t *line, int *column);
};

line_t *create_line();
void destroy_line(line_t *line);


#endif
