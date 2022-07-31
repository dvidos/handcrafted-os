#ifndef _VIEWPORT_H
#define _VIEWPORT_H

typedef struct viewport_priv_data viewport_priv_data_t;
typedef struct viewport_ops viewport_ops_t;

typedef struct viewport {
    viewport_ops_t *ops;
    viewport_priv_data_t *priv_data;

    buffer_t *buffer;
    int visible_rows;
    int visible_cols;
    int first_visible_row; // of text, zero based
    int first_visible_col;
} viewport_t;

struct viewport_ops {
    int (*repaint)(viewport_t *vp);
    int (*ensure_cursor_visible)(viewport_t *vp);
};

viewport_t *create_multi_line_buffer_viewport(buffer_t *buffer);
void destroy_multi_line_buffer_viewport(viewport_t *vp);

#endif
