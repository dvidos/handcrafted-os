#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "view.h"


static void draw_buffer(view_t *view) {
    for (int y = 0; y < view->screen_rows - 1; y++) {
        int line_num = view->first_visible_row + y;
        if (line_num >= 0 && line_num < view->buffer->lines_count) {
            line_t *line = view->buffer->lines[line_num];
            goto_xy(0, y);
            int displayed_chars = 0;
            if (view->first_visible_col < line->length) {
                // if this exceeds screen line, it will wrap. bad...
                puts(&line->buffer[view->first_visible_col]);
                displayed_chars = line->length - view->first_visible_col;
            }

            int remaining_chars = view->screen_cols - displayed_chars;
            while (remaining_chars-- < 0)
                putchar(' ');
        } else {
            goto_xy(0, y);
            puts("~                                                                               ");
        }
    }
    goto_xy(
        view->buffer->col_number - view->first_visible_col,
        view->buffer->row_number - view->first_visible_row
    );
}

static void draw_footer(view_t *view, char *cmd) {
    goto_xy(0, view->screen_rows - 1);
    printf("%12s      Ln %d, Col %d, %d%%", 
        cmd,
        view->buffer->row_number,
        view->buffer->col_number,
        view->first_visible_row * 100 / view->buffer->lines_count
    );
}

view_t *create_view(buffer_t *buffer) {
    view_t *view = malloc(sizeof(view_t));
    memset(view, 0, sizeof(view_t));

    screen_dimensions(&view->screen_cols, &view->screen_rows);

    view_ops_t *ops = malloc(sizeof(view_ops_t));
    ops->draw_buffer = draw_buffer;
    ops->draw_footer = draw_footer;

    view->buffer = buffer;
    view->ops = ops;

    return view;
}

void destroy_view(view_t *view) {
    if (view->buffer)
        destroy_buffer(view->buffer);
    if (view->ops)
        free(view->ops);
    free(view);
}
