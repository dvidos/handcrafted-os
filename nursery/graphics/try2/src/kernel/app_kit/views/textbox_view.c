#include "textbox_view.h"


static void _paint(view_t *v, graphics_context_t *gc, area dirty) {
    textbox_view *t = (textbox_view *)v;

    color c = t->focused ? color_gray_of(0xFF) : color_gray_of(0xCC);
    gc_set_fill(gc, color_params_solid(c));
    gc_draw_rect(gc, v->bounds);

    gc_set_text(gc, text_params_of(geneva9, ALIGN_MIDDLE_CENTER));
    gc_draw_text(gc, t->buffer, v->bounds);

    gc_set_stroke(gc, color_gray_of(0x55), 1);
    gc_draw_border(gc, v->bounds);
}

static bool _on_key_event(view_t *v, key_event_t e) {
    textbox_view *t = (textbox_view *)v;

    if (e.ascii != 0 && strlen(t->buffer) < sizeof(t->buffer) - 1) {
        t->buffer[strlen(t->buffer)] = e.ascii;
        v->callbacks->invalidate(v, v->bounds);
    } else if (e.keycode == KEY_BACKSPACE && strlen(t->buffer) > 0) {
        t->buffer[strlen(t->buffer)] = 0;
        v->callbacks->invalidate(v, v->bounds);
    }
}

textbox_view *new_textbox_view() {
    textbox_view *t = (textbox_view *)kmalloc(sizeof(textbox_view));

    view_base_initialize(&t->base);
    t->base.callbacks->paint = _paint;
    t->base.callbacks->on_key_event = _on_key_event;
    t->base.focusable = true;

    return t;
}

void textbox_view_set_text(textbox_view *t, const char *text) {
}
