#include "../../memory/malloc.h"
#include "../../concepts/logger.h"
#include "background_view.h"


static void _background_view_paint(view_t *v, graphics_context_t *gc, area dirty) {
    LOG_TRACE();
    const ui_style_t *style = ui_style();

    gc_set_fill(gc, style->window.bg);
    gc_draw_rect(gc, v->bounds);

    view_paint_children(v, gc, dirty);
}

background_view *new_background_view() {
    background_view *b = (background_view *)kmalloc(sizeof(background_view));

    view_base_initialize(&b->base, "root_view");
    b->base.callbacks.paint = _background_view_paint;
    b->base.focusable = false;

    return b;
}

