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

    gc_set_border(gc, BORDER_SUNKEN, style->control.border_color, 2, style->control.contrast_3d);
    gc_draw_border(gc, v->bounds);
}

static bool _textbox_view_on_key_event(view_t *v, key_event_t e) {
    textbox_view *t = (textbox_view *)v;
    if (e.type != KEY_PRESSED)
        return false;
    
    if ((e.ascii >= 32 && e.ascii < 127) && strlen(t->buffer) < sizeof(t->buffer) - 1) {
        t->buffer[strlen(t->buffer)] = e.ascii;
        view_invalidate(v);
        return true;

    } else if (e.keycode == KEY_BACKSPACE) {
        int len = strlen(t->buffer);
        if (len == 0) return false;
    
        t->buffer[len - 1] = 0;
        view_invalidate(v);
        return true;
    }
}

textbox_view *new_textbox_view() {
    LOG_TRACE();

    textbox_view *t = (textbox_view *)kmalloc(sizeof(textbox_view));

    view_base_initialize(&t->base, "textbox_view");
    t->base.callbacks.paint = _textbox_view_paint;
    t->base.callbacks.on_key_event = _textbox_view_on_key_event;
    t->base.focusable = true;

    strcpy(t->buffer, "hello-AMgy");

    return t;
}

void textbox_view_set_text(textbox_view *t, const char *text) {
    if (strcmp(t->buffer, text) == 0)
        return;
    
    memcpy(t->buffer, text, min(strlen(text) + 1, sizeof(t->buffer)));
    t->buffer[sizeof(t->buffer) - 1] = 0;
    view_invalidate((view_t *)t);
}
