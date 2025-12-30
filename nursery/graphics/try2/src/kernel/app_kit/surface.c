#include "surface.h"
#include "../memory/malloc.h"


surface_t *new_surface(int w, int h, surface_role_t role) {
    surface_t *s = kmalloc(sizeof(surface_t));
    memset(s, 0, sizeof(*s));

    s->role         = role;
    s->frame        = area_of(0, 0, w, h);
    s->dirty_area   = area_of(0, 0, w, h);
    s->needs_redraw = true;

    s->is_visible = true;
    s->is_opaque  = true;

    s->z_order = 0;

    s->buffer = new_gbuffer(w, h);

    return s;
}

void surface_destroy(surface_t *s) {
    if (!s) return;
    gb_free(s->buffer);
    kfree(s);
}


void surface_set_position(surface_t *s, int x, int y) {
    if (s->frame.x == x && s->frame.y == y) return;

    s->frame.x = x;
    s->frame.y = y;
    s->needs_redraw = true;
}

void surface_set_size(surface_t *s, int w, int h) {
    if (s->frame.width == w && s->frame.height == h) return;

    gbuffer *oldb = s->buffer;
    gbuffer *newb = new_gbuffer(w, h);

    int copy_w = min(oldb->area.width,  w);
    int copy_h = min(oldb->area.height, h);

    gb_copy_area_fast(newb, oldb, size_of(copy_w, copy_h), point_zero(), point_zero());
    gb_free(oldb);

    s->buffer = newb;
    s->frame.width  = w;
    s->frame.height = h;
    s->dirty_area = area_of(0, 0, w, h);
    s->needs_redraw = true;
}

void surface_show(surface_t *s) { 
    if (s->is_visible) return;

    s->is_visible = true;
    s->needs_redraw = true;
}

void surface_hide(surface_t *s) { 
    if (!s->is_visible) return;

    s->is_visible = false;
    s->needs_redraw = true;
}

void surface_damage_area(surface_t *s, area a) {
    if (area_is_empty(a)) return;

    // damage area is local to surface
    s->dirty_area = area_union(s->dirty_area, a); 
    s->needs_redraw = true;
}

void surface_damage_all(surface_t *s) {
    s->dirty_area = s->frame;
    s->needs_redraw = true;
}

void surface_begin_draw(surface_t *s, graphics_context_t *gc) {
    // gc is assumed already bound to s->buffer
    // surface only resets damage bookkeeping
}

void surface_end_draw(surface_t *s) {
    // nothing else to do here
    // SM/WM will consume dirty_area + needs_redraw
}

void surface_raise(surface_t *s) {
    s->z_order += 1;
}

void surface_lower(surface_t *s) {
    s->z_order -= 1;
}
 
void surface_set_z(surface_t *s, int z) {
    if (s->z_order == z) return;

    s->z_order = z;
    s->needs_redraw = true;
}
