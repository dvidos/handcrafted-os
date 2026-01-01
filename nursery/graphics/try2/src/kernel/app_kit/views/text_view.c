#include "text_view.h"


static void _paint(view_t *v, graphics_context_t *gc, area dirty) {
    // draw the button, different if focused and if pressed
    text_view *t = (text_view *)v;
    gc_draw_text(gc, t->text, t->base.bounds);
}

text_view *new_text_view(const char *text) {
    text_view *v = (text_view *)kmalloc(sizeof(text_view));

    view_base_initialize(&v->base);
    v->base.callbacks->paint = _paint;
    v->text = text;
}

