#include "../app_kit/surface.h"
#include "../app_kit/layout_helper.h"
#include "../app_kit/views/text_view.h"
#include "../concepts/screen_manager.h"



static border_style_t border_styles[] = { BORDER_NONE, BORDER_FLAT, BORDER_RAISED, BORDER_SUNKEN, BORDER_GROOVE, BORDER_RIDGE };
static color_fill_type fills[] = { FILL_TYPE_NONE, FILL_TYPE_SOLID, FILL_TYPE_LINEAR_GRADIENT };
static int radii[] = { 0, 1, 2, 3, 5, 10, 20, 40, 80 };
static int thicknesses[] = { 0, 1, 2, 3, 5, 10, 20, 40 };
static factor contrasts[] = { 0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f };

struct demo_settings {
    view_t *demo_view;

    int border_idx;
    int radious_idx;
    int thickness_idx;
    int fill_idx;
    int contrast_idx;

    fill_params fill_pars;
    border_params border_pars;
    bool show_alpha;
    bool show_sections;
};

static struct demo_settings ds;

static void _handle_key(surface_t *s, key_event_t e) {
    if (e.type != KEY_PRESSED) return;
    bool redraw = false;
    
    switch (e.ascii) {
        case 'b':
            ds.border_idx = (ds.border_idx + 1) % (sizeof(border_styles)/sizeof(border_styles[0]));
            ds.border_pars.style = border_styles[ds.border_idx];
            redraw = true;
            break;
        case 'r':
            ds.radious_idx = (ds.radious_idx + 1) % (sizeof(radii)/sizeof(radii[0]));
            ds.border_pars.radius = radii[ds.radious_idx];
            redraw = true;
            break;
        case 't':
            ds.thickness_idx = (ds.thickness_idx + 1) % (sizeof(thicknesses)/sizeof(thicknesses[0]));
            ds.border_pars.thickness = thicknesses[ds.thickness_idx];
            redraw = true;
            break;
        case 'a':
            ds.show_alpha = !ds.show_alpha;
            redraw = true;
            break;
        case 'c': // contrast
            ds.contrast_idx = (ds.contrast_idx + 1) % (sizeof(contrasts)/sizeof(contrasts[0]));
            ds.border_pars.contrast_3d = contrasts[ds.contrast_idx];
            redraw = true;
            break;
        case 'v':
            ds.show_sections = !ds.show_sections;
            redraw = true;
            break;
        case 'f':
            ds.fill_idx = (ds.fill_idx + 1) % (sizeof(fills)/sizeof(fills[0]));
            ds.fill_pars.fill_type = fills[ds.fill_idx];
            redraw = true;
            break;
    }

    if (redraw)
        view_invalidate(ds.demo_view);
}

static void _paint_demo_view(view_t *v, graphics_context_t *gc, area dirty) {
    // allow 20 pixels for shadow
    area r = area_grow(v->bounds, -20, -20);

    gc_show_sections(gc, ds.show_sections);
    gc_show_alpha(gc, ds.show_alpha);
    gc_set_fill(gc, ds.fill_pars);
    gc_set_border(gc, border_styles[ds.border_idx], color_tango_blue(), thicknesses[ds.thickness_idx], ds.border_pars.contrast_3d);
    gc_set_roundness(gc, radii[ds.radious_idx]);

    gc_draw_rect(gc, r);
    gc_draw_border(gc, r);
}

surface_t *create_graphics_demo_surface() {
    int w = 700;
    int h = 500;

    memset(&ds, 0, sizeof(ds));
    ds.border_idx = 1;
    ds.fill_idx = 1;
    ds.radious_idx = 2;
    ds.thickness_idx = 2;
    ds.fill_pars = (fill_params){ .clr = color_tango_green(), .clr2 = color_tango_yellow(), .fill_type = FILL_TYPE_SOLID, .gradient_p1 = (point){0,0}, .gradient_p2 = (point){50,100}, .ease = ease_linear };
    ds.border_pars = (border_params){ .clr = color_tango_magenta(), .contrast_3d = 0.5f, .radius = 0, .thickness = 1, .style = BORDER_FLAT };

    const ui_style_t *style = ui_style();
    vert_layout_t vl = new_vert_layout(w, 10, 10);

    surface_t *s = new_surface(w+20, h+20, SURFACE_WINDOW, true, "graphics_demo");
    surface_set_key_handler(s, _handle_key);

    ds.demo_view = new_base_view();
    ds.demo_view->callbacks.paint = _paint_demo_view;
    view_t *instructions_view = (view_t *)new_text_view("[B]order, [R]adius, [T]hickness, [F]ill, [A]lpha, [V]isual-sections");

    surface_add_view(s, ds.demo_view);
    surface_add_view(s, (view_t *)instructions_view);

    vert_layout_add(&vl, ds.demo_view, h-40);
    vert_layout_add(&vl, instructions_view, 20);

    area a = screen_manager_center_on_screen(vert_layout_boundaries(&vl));
    surface_set_size(s, a.width, a.height);
    surface_set_position(s, a.x, a.y);

    return s;
}