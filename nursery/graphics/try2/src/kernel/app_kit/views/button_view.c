#include "../../memory/malloc.h"
#include "../../concepts/logger.h"
#include "button_view.h"


static void _button_view_paint(view_t *v, graphics_context_t *gc, area dirty) {
    LOG_TRACE();
    
    // draw the button, different if focused and if pressed
    button_view *b = (button_view *)v;
    const ui_style_t *style = ui_style();
    
    gc_set_roundness(gc, 5);

    color c = style->control.bg.clr;
    if (v->focused) c = color_lighten(c, 0.2);
    if (b->pressed) fill_params_solid(style->control.dark);

    gc_set_fill(gc, fill_params_solid(c));
    gc_draw_rect(gc, v->bounds);

    gc_set_text(gc, text_params_of(style->control.text.font, ALIGN_MIDDLE_CENTER, style->control.text.color));
    gc_draw_text(gc, b->label, v->bounds);

    gc_set_stroke(gc, style->control.border_style, style->control.border_color,  v->focused ? 2 : 1);
    gc_draw_border(gc, v->bounds);
}

static bool _button_view_on_mouse_event(view_t *v, mouse_event_t e) {
    // track clicked or not
    // set clicked = ... then invalidate
    button_view *b = (button_view *)v;

    if (e.type == MOUSE_LBTN_DOWN) {
        b->pressed = true;
        view_invalidate(v);

    } else if (e.type == MOUSE_LBTN_UP) {
        b->pressed = false;
        view_invalidate(v);

        // trigger the event handler
        b->on_click(b->userdata);
    }

    // how to detect we moved outside of our bounds?
    // we need to capture mouse... another chapter.
}

static bool _button_view_on_key_event(view_t *v, key_event_t e) {
    button_view *b = (button_view *)v;

    if (e.type == KEY_PRESSED && e.keymods == 0 && e.keycode == KEY_SPACE) {
        b->on_click(b->userdata);
    }
}

button_view *new_button_view(const char *label, click_handling_func *on_click, void *userdata) {
    button_view *b = (button_view *)kmalloc(sizeof(button_view));
    memset(b, 0, sizeof(button_view));

    view_base_initialize(&b->base, "button_view");
    b->base.callbacks.paint = _button_view_paint;
    b->base.callbacks.on_mouse_event = _button_view_on_mouse_event;
    b->base.callbacks.on_key_event = _button_view_on_key_event;
    b->base.focusable = true;

    b->label = label;
    b->on_click = on_click;
    b->userdata = userdata;

    return b;
}

button_view *new_default_button_view(const char *label, click_handling_func *on_click, void *userdata) {
    button_view *v = new_button_view(label, on_click, userdata);
    v->is_default = true;
    return v;
}

button_view *new_cancel_button_view(const char *label, click_handling_func *on_click, void *userdata) {
    button_view *v = new_button_view(label, on_click, userdata);
    v->is_cancel = true;
    return v;
}

void button_view_perform_click(button_view *v) {
    if (v->on_click != NULL)
        v->on_click(v->userdata);
}