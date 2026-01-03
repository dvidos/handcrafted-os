#include "../../memory/malloc.h"
#include "../../concepts/logger.h"
#include "button_view.h"


static void _button_view_paint(view_t *v, graphics_context_t *gc, area dirty) {
    LOG_TRACE();
    
    // draw the button, different if focused and if pressed
    button_view *b = (button_view *)v;
    const ui_style_t *style = ui_style();

    gc_set_fill(gc, b->pressed ? fill_params_solid(style->control.dark) : style->control.bg);
    gc_draw_rect(gc, v->bounds);

    gc_set_text(gc, text_params_of(style->control.text.font, ALIGN_MIDDLE_CENTER, style->control.text.color));
    gc_draw_text(gc, b->label, v->bounds);

    gc_set_stroke(gc, style->control.border_color, v->focused ? 2 : 1);
    gc_draw_border(gc, v->bounds);
}

static bool _button_view_on_mouse_event(view_t *v, mouse_event_t e) {
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
    b->base.callbacks.paint = _button_view_paint;
    b->base.callbacks.on_mouse_event = _button_view_on_mouse_event;
    b->base.focusable = true;

    b->label = label;
    b->on_click = on_click;
    b->userdata = userdata;

    return b;
}

