#ifndef _VIEW_H
#define _VIEW_H

#include "buffer.h"

typedef struct view_ops view_ops_t;

typedef struct view {
    buffer_t *buffer;
    int screen_rows;
    int screen_cols;
    int first_visible_row;
    int first_visible_col;

    view_ops_t *ops;
} view_t;

struct view_ops {
    void (*draw_buffer)(view_t *view);
    void (*draw_footer)(view_t *view, char *cmd);
};

view_t *create_view(buffer_t *buffer);
void destroy_view(view_t *view);





#endif
