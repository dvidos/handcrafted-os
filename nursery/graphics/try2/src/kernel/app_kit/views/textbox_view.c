#include "../../concepts/logger.h"
#include "textbox_view.h"


static void _textbox_view_paint(view_t *v, graphics_context_t *gc, area dirty) {
    textbox_view *t = (textbox_view *)v;
    const ui_style_t *style = ui_style();

    fill_params bg = v->focused ? style->control.input_bg : style->window.bg;
    gc_set_fill(gc, bg);
    gc_draw_rect(gc, v->bounds);

    gc_set_text(gc, style->control.text);
    gc_draw_text(gc, t->buffer, v->bounds);

    gc_set_stroke(gc, style->control.border_color, style->control.border_thickness);
    gc_draw_border(gc, v->bounds);
}

static bool _textbox_view_on_key_event(view_t *v, key_event_t e) {
    textbox_view *t = (textbox_view *)v;

    if (e.ascii != 0 && strlen(t->buffer) < sizeof(t->buffer) - 1) {
        t->buffer[strlen(t->buffer)] = e.ascii;
        view_invalidate(v);

    } else if (e.keycode == KEY_BACKSPACE && strlen(t->buffer) > 0) {
        t->buffer[strlen(t->buffer)] = 0;
        view_invalidate(v);
    }
}

textbox_view *new_textbox_view() {
    LOG_TRACE();

    textbox_view *t = (textbox_view *)kmalloc(sizeof(textbox_view));

    view_base_initialize(&t->base);
    t->base.callbacks.paint = _textbox_view_paint;
    t->base.callbacks.on_key_event = _textbox_view_on_key_event;
    t->base.focusable = true;

    return t;
}

void textbox_view_set_text(textbox_view *t, const char *text) {
    memcpy(t->buffer, text, min(strlen(text) + 1, sizeof(t->buffer)));
    t->buffer[sizeof(t->buffer) - 1] = 0;

    view_invalidate((view_t *)t);
}
