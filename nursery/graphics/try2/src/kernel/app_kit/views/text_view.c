#include "../../concepts/logger.h"
#include "text_view.h"


static void _text_view_paint(view_t *v, graphics_context_t *gc, area dirty) {
    LOG_TRACE();
    
    // draw the button, different if focused and if pressed
    text_view *t = (text_view *)v;
    gc_draw_text(gc, t->text, t->base.bounds);
}

text_view *new_text_view(const char *text) {
    text_view *v = (text_view *)kmalloc(sizeof(text_view));

    view_base_initialize(&v->base);
    v->base.callbacks.paint = _text_view_paint;
    v->text = text;

    return v;
}

