#pragma once
#include "view.h"

typedef struct vert_layout_t {
    int padding;
    int spacing;
    int content_width;
    int rows_count;
    int cursor_x;
    int cursor_y;
} vert_layout_t;
vert_layout_t new_vert_layout(int views_width, int padding, int spacing);
void vert_layout_add(vert_layout_t *l, view_t *v, int height);
void vert_layout_add_split(vert_layout_t *l, view_t *v1, view_t *v2, int height, float split_factor);
area vert_layout_boundaries(vert_layout_t *l);


typedef struct horiz_layout_t {
    int padding;
    int spacing;
    int content_height;
    int columns_count;
    int cursor_x;
    int cursor_y;
} horiz_layout_t;    

horiz_layout_t new_horiz_layout(int views_height, int padding, int spacing);
void horiz_layout_add(horiz_layout_t *l, view_t *v, int width);
area horiz_layout_boundaries(horiz_layout_t *l);
