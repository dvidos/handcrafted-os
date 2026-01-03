#include "layout_helper.h"
#include "ui_style.h"


vert_layout_t new_vert_layout(int content_width, int padding, int spacing) {
    return (vert_layout_t){
        .content_width = content_width,
        .padding = padding,
        .spacing = spacing,
        .count = 0,
        .cursor_x = padding,
        .cursor_y = padding,
    };
}

void vert_layout_add(vert_layout_t *l, view_t *v, int height) {
    if (l->count > 0)
        l->cursor_y += l->spacing;
    view_set_frame(v, area_of(l->cursor_x, l->cursor_y, l->content_width, height));
    l->cursor_y += height;
    l->count++;
}

area vert_layout_boundaries(vert_layout_t *l) {
    return area_of(0, 0, l->content_width + l->padding * 2, l->cursor_y + l->padding);
}


horiz_layout_t new_horiz_layout(int content_height, int padding, int spacing) {
    return (horiz_layout_t){
        .content_height = content_height,
        .padding = padding,
        .spacing = spacing,
        .count = 0,
        .cursor_x = padding,
        .cursor_y = padding,
    };
}

void horiz_layout_add(horiz_layout_t *l, view_t *v, int width) {
    if (l->count > 0)
        l->cursor_x += l->spacing;
    view_set_frame(v, area_of(l->cursor_x, l->cursor_y, width, l->content_height));
    l->cursor_x += width;
    l->count++;
}

area horiz_layout_boundaries(horiz_layout_t *l) {
    return area_of(0, 0, l->cursor_x + l->padding, l->content_height + l->padding * 2);
}
