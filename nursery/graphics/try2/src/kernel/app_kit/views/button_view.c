#include "button_view.h"


static void _paint(view_t *v, graphics_context_t *gc, area dirty) {
    // draw the button, different if focused and if pressed
    button_view *b = (button_view *)v;

    color c = b->pressed ? color_gray_of(0xAA) : color_gray_of(0xCC);
    gc_set_fill(gc, color_params_solid(c));
    gc_draw_rect(gc, v->bounds);

    gc_set_text(gc, text_params_of(geneva9, ALIGN_MIDDLE_CENTER));
    gc_draw_text(gc, b->label, v->bounds);

    gc_set_stroke(gc, color_gray_of(0x55), v->focused ? 2 : 1);
    gc_draw_border(gc, v->bounds);
}

static bool _on_mouse_event(view_t *v, mouse_event_t e) {
    // track clicked or not
    // set clicked = ... then invalidate
    button_view *b = (button_view *)v;

    if (e.type == MOUSE_LBTN_DOWN) {
        b->pressed = true;
        view_mark_all_dirty(v);

    } else if (e.type == MOUSE_LBTN_UP) {
        b->pressed = false;
        view_mark_all_dirty(v);

        // trigger the event handler
        b->on_click(b->userdata);
    }

    // how to detect we moved outside of our bounds?
    // we need to capture mouse... another chapter.
}


button_view *new_button_view(const char *label, click_handling_func *on_click, void *userdata) {
    button_view *b = (button_view *)kmalloc(sizeof(button_view));

    view_base_initialize(&b->base);
    b->base.callbacks->paint = _paint;
    b->base.callbacks->on_mouse_event = _on_mouse_event;
    b->base.focusable = true;

    b->label = label;
    b->on_click = on_click;
    b->userdata = userdata;

    return b;
}

