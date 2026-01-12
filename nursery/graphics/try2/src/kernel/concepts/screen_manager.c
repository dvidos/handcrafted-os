#include "../fundamentals.h"
#include "../graphics/geometry.h"
#include "../memory/string.h"
#include "../graphics/cursors/mouse_cursor.h"
#include "screen_manager.h"
#include "../app_kit/surface.h"
#include "../app_kit/graphics_context.h"
#include "logger.h"
#include "../algorithms/rand.h"
#include "../containers/dllist.h"
#include "../cpu/timer.h"

#define MAX_SURFACES                 64  // this will be dynamic one day...
#define FOCUSED_SURFACES_STACK_SIZE  16  // stack of modal focused surfaces
#define MAX_DIRTY_AREAS              32  // this one too...


typedef struct mouse_info {
    point curr_pos;         // 0,0 is top left, not screen center
    size cursor_size;       // cursor size
    point hotspot_offset;   // 0,0 is top left of the cursor graphic
    gbuffer *pointer;        // masked? alpha? something fast?
    gbuffer *saved_bg;          // saved pixels under cursor
    point saved_bg_origin;     // where the pixels were saved from (maybe mouse moved since)
    point restored_bg_origin;  // where we restored, so we can copy to actual framebuffer
    bool is_visible;
    bool needs_redraw;
} mouse_info_t;

typedef struct screen_info {
    void *fb_address;
    int width;
    int height;
    int pitch;
    int bpp;
} screen_info_t;

typedef struct screen_manager {
    // Framebuffer info
    screen_info_t screen;

    // what we draw to
    gbuffer *backbuffer;
    mouse_info_t mouse;

    // dirty areas, only these are painted, for performance
    int needs_repaint;
    area dirty_areas[MAX_DIRTY_AREAS];
    int dirty_area_count;

    // event distribution:
    dlist_t surfaces; // head = top
    surface_t *mouse_capture; // nullable
    surface_t *focused_surface; // nullable

    int visual_debug;

} screen_manager_t;
static screen_manager_t sm;

void initialize_screen_manager(void *framebuffer, int width, int height, int pitch, int bpp) {
    memset(&sm, 0, sizeof(screen_manager_t));

    sm.screen.fb_address = framebuffer;
    sm.screen.width = width;
    sm.screen.height = height;
    sm.screen.pitch = pitch;
    sm.screen.bpp = bpp;

    sm.backbuffer = new_gbuffer(width, height);
    gb_fill(sm.backbuffer, 0x3f9fbf);

    sm.mouse.curr_pos = point_of(sm.screen.width / 2, sm.screen.height / 2);
    sm.mouse.is_visible = 1;
    sm.mouse.cursor_size = size_of(32, 32);
    sm.mouse.saved_bg = new_gbuffer(sm.mouse.cursor_size.width, sm.mouse.cursor_size.height);
    sm.mouse.saved_bg_origin = point_of(-1, -1); // signal not captured yet.
    sm.mouse.restored_bg_origin = point_of(-1, -1);

    // have list of surfaces, without knowing their internals
    dlist_init(&sm.surfaces, surface_get_dlist_node_offset());

    screen_manager_mark_area_dirty(sm.backbuffer->area); // for first drawing, this should move to desktop composer
}

// -----------------------------------------------------------------------

static void _surface_invalidated(surface_t *surface, area dirty) {
    if (!surface_is_visible(surface))
        return;
    screen_manager_mark_area_dirty(dirty);
}

static surface_owner_interface_t surfaces_interface = {
    .surface_invalidated = _surface_invalidated,
};

// -----------------------------------------------------------------------

size screen_manager_get_screen_size() {
    return size_of(sm.screen.width, sm.screen.height);
}

area screen_manager_get_screen_area() {
    return area_of(0, 0, sm.screen.width, sm.screen.height);
}

area screen_manager_center_on_screen(area a) {
    return area_of(
        (sm.screen.width - a.width) / 2,
        (sm.screen.height - a.height) / 2,
        a.width,
        a.height
    );
}

// ----------------------------------------------------------------------

static surface_t *screen_manager_get_top_focusable_surface() {
    // from top to bottom
    dlist_foreach(&sm.surfaces, surface_t, s) {
        if (!surface_is_visible(s) || !surface_is_focusable(s)) 
            continue;
        
        return s;
    }
    return NULL;
}

static void screen_manager_focus_surface(surface_t *s) {
    if (s != NULL) {
        if (sm.focused_surface == s || !surface_is_focusable(s))
            return;
    }   

    // if previous exists, notify
    if (sm.focused_surface)
        surface_on_focus_lost(sm.focused_surface);

    sm.focused_surface = s;
    if (sm.focused_surface != NULL) {
        surface_on_focus_gained(sm.focused_surface);
    }
}

bool screen_manager_add_surface(surface_t *s) {
    LOG_TRACE();
    dlist_prepend(&sm.surfaces, s);

    // need to redraw this area
    surface_set_owner_interface(s, &surfaces_interface);
    screen_manager_mark_area_dirty(surface_get_frame(s));

    surface_on_shown(s); // actually will be painted in a bit.
    if (surface_is_focusable(s))
        screen_manager_focus_surface(s);
    
    LOG_TRACE();
    return true;
}

bool screen_manager_remove_surface(surface_t *s) {

    // we must mark all the surfaces below "s" as dirty, using the frame of s.
    // so they are redrawn
    area exposed = surface_get_frame(s);
    dlist_foreach_reverse(&sm.surfaces, surface_t, below) {
        const char *d = surface_get_debug_info(below);
        if (below == s)
            break;

        exposed = area_intersect(exposed, surface_get_frame(below));
        if (area_is_empty(exposed))
            continue;
        surface_invalidate_area(below, area_to_local(exposed, surface_get_frame(below)));
    }

    dlist_remove(&sm.surfaces, s);

    surface_on_hidden(s);
    
    // need to redraw this area
    screen_manager_mark_area_dirty(surface_get_frame(s));
    surface_set_owner_interface(s, NULL);

    // remember to clear/move the focus
    surface_t * top_focusable = screen_manager_get_top_focusable_surface();
    screen_manager_focus_surface(top_focusable); // even if none

    return true;
}

void screen_manager_bring_surface_to_top(surface_t *s) {
    dlist_move_to_head(&sm.surfaces, s);

    if (surface_is_focusable(s))
        screen_manager_focus_surface(s);
}

void screen_manager_push_surface_to_bottom(surface_t *s) {
    dlist_move_to_tail(&sm.surfaces, s);

    // remember to clear/move the focus
    surface_t * top_focusable = screen_manager_get_top_focusable_surface();
    screen_manager_focus_surface(top_focusable); // even if none
}


void screen_manager_mark_area_dirty(area area) {
    // essentially, this is the only way to get to show something on screen!
    // only if marked dirty, will it be asked to be painted, and will end up on screen!

    if (sm.dirty_area_count >= MAX_DIRTY_AREAS) {
        // this should be dynamic but for now, we can do the following, 
        // to force whole screen, even if not performant
        sm.dirty_areas[MAX_DIRTY_AREAS - 1] = area_union(sm.dirty_areas[MAX_DIRTY_AREAS - 1], area);
    } else {
        sm.dirty_areas[sm.dirty_area_count++] = area;
    }
    sm.needs_repaint = true;
}

// --------------------------------------------------------------------------

static inline area get_mouse_cursor_area(int x, int y) {
    return area_of(
        x - sm.mouse.hotspot_offset.x,
        y - sm.mouse.hotspot_offset.y,
        sm.mouse.cursor_size.width,
        sm.mouse.cursor_size.height
    );
}

void screen_manager_intercept_mouse_movement(event_t *ev) {
    if (ev->type == MOUSE_MOVED) {
        // update screen manager
        #ifdef HOSTED_ENV
            // in SDL, we get mouse position from the library
            sm.mouse.curr_pos.x = ev->mouse.pos.x;
            sm.mouse.curr_pos.y = ev->mouse.pos.y;
        #else
            // in bare metal, we get deltas from the PS/2 packet
            sm.mouse.curr_pos.x += ev->mouse.delta.dx;
            sm.mouse.curr_pos.y += ev->mouse.delta.dy;
        #endif
        sm.mouse.curr_pos.x = clamp(sm.mouse.curr_pos.x, 0, sm.screen.width - 1);
        sm.mouse.curr_pos.y = clamp(sm.mouse.curr_pos.y, 0, sm.screen.height - 1);
        sm.mouse.needs_redraw = true; // we don't mark dirty, as _we_ handle the bg buffer for this
        sm.needs_repaint = true; 
    }

    // in any case, enrich the event with current position
    ev->mouse.pos = sm.mouse.curr_pos;
}

void screen_manager_set_mouse_position(int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > sm.screen.width - 1) x = sm.screen.width - 1;
    if (y > sm.screen.height - 1) y = sm.screen.height - 1;
    
    if (x == sm.mouse.curr_pos.x && y == sm.mouse.curr_pos.y)
        return;
    
    // mark old and new areas as dirty, to repaint them.
    sm.mouse.curr_pos.x = x;
    sm.mouse.curr_pos.y = y;
    sm.mouse.needs_redraw = true; // we don't mark dirty, as _we_ handle the bg buffer for this
    sm.needs_repaint = true; 
}

void screen_manager_get_mouse_position(int *x, int *y) {
    *x = sm.mouse.curr_pos.x;
    *y = sm.mouse.curr_pos.y;
}

void screen_manager_set_mouse_visible(bool visible) {
    sm.mouse.is_visible = visible;
    sm.mouse.needs_redraw = true;
    sm.needs_repaint = true; 
}

bool screen_manager_get_mouse_visible() {
    return sm.mouse.is_visible;
}

static void restore_mouse_cursor_area() {
    if (sm.mouse.saved_bg_origin.x == -1 && sm.mouse.saved_bg_origin.y == -1) {
        log.trace("not restoring mouse cursor");
        return; // not captured yet.
    }
    
    log.trace("restoring mouse cursor at (%d,%d)", sm.mouse.saved_bg_origin.x, sm.mouse.saved_bg_origin.y);
    gb_copy_area_fast(sm.backbuffer, sm.mouse.saved_bg, sm.mouse.cursor_size, sm.mouse.saved_bg_origin, point_zero());

    // keep this so we can copy the rectangle from backbuffer to fb
    sm.mouse.restored_bg_origin = sm.mouse.saved_bg_origin;
}

static void draw_mouse_cursor() {
    if (!sm.mouse.is_visible)
        return;
    
    area mouse_area = get_mouse_cursor_area(sm.mouse.curr_pos.x, sm.mouse.curr_pos.y);
    log.trace("preserving mouse cursor area at (%d,%d,%d,%d)", mouse_area.x, mouse_area.y, mouse_area.width, mouse_area.height);
    gb_copy_area_fast(sm.mouse.saved_bg, sm.backbuffer, area_size(mouse_area), point_zero(), area_location(mouse_area));
    sm.mouse.saved_bg_origin = area_location(mouse_area);

    log.trace("drawing mouse cursor at (%d,%d)", mouse_area.x, mouse_area.y);
    gb_draw_cursor32_fast(sm.backbuffer, sm.mouse.curr_pos, &windows_cursor); // &triangle_cursor);
}

void screen_manager_set_mouse_capture(surface_t *s) {
    if (sm.mouse_capture == s)
        return;

    if (sm.mouse_capture)
        log.warn("mouse_capture requested while another surface already has it. consider making a stack");

    sm.mouse_capture = s;
}

void screen_manager_clear_mouse_capture() {
    if (sm.mouse_capture == 0) {
        log.warn("clear mouse_capture requested while already clear");
        return;
    }
    sm.mouse_capture = 0;
}

void screen_manager_dispatch_key_event(key_event_t e) {
    if (sm.focused_surface == NULL) {
        // log.debug("No surface found to handle keyboard event");
        return;
    }
    
    // by definition, the focused surface is the one to receive the keys events (hence the "focus" noun)
    surface_handle_key_event(sm.focused_surface, e);
}

surface_t *screen_manager_hit_test(point mouse_pos) {
    // from top to bottom
    dlist_foreach(&sm.surfaces, surface_t, s) {
        if (!surface_is_visible(s))
            continue;
        if (!point_is_inside(mouse_pos, surface_get_frame(s)))
            continue;
        
        if (!surface_is_visible(s) || !surface_accepts_mouse(s)) 
            continue;
        
        return s;
    }
    return NULL;
}

void screen_manager_dispatch_mouse_event(mouse_event_t e) {
    surface_t *s = NULL;
    if (sm.mouse_capture)
        s = sm.mouse_capture;
    else
        s = screen_manager_hit_test(e.pos);

    if (s != NULL)
        surface_handle_mouse_event(s, mouse_event_localized(e, surface_get_frame(s)));
}

// --------------------------------------------------------------------------

static void copy_backbuffer_to_physical_framebuffer() {
    area area;

    // to see what the saved bg contains
    // area = area_of(5, 5, 32, 32);
    // gb_copy_area_fast(sm.backbuffer, sm.mouse.saved_bg, area_size(area), area_location(area), point_zero());
    // gb_border(sm.backbuffer, area, area, 0, 1, 0xFFFFFFFF);
    // screen_manager_mark_area_dirty(area);
    
    // we are only copying what is dirty and has changed
    // otherwise, copying the entire screen is *too* slow
    for (int i = 0; i < sm.dirty_area_count; i++)
        gb_copy_area_to_framebuffer_with_bpp(sm.backbuffer, sm.dirty_areas[i], sm.screen.fb_address, sm.screen.pitch, sm.screen.bpp);        
    sm.dirty_area_count = 0;

    // we also copy the area of the old and new mouse positions
    if (sm.mouse.needs_redraw) {
        area = area_with(point_of(sm.mouse.restored_bg_origin.x, sm.mouse.restored_bg_origin.y), sm.mouse.cursor_size);
        log.trace("copying restored mouse backbuffer to fb at (%d,%d)", area.x, area.y);
        gb_copy_area_to_framebuffer_with_bpp(sm.backbuffer, area, sm.screen.fb_address, sm.screen.pitch, sm.screen.bpp);        

        if (sm.mouse.is_visible) {
            area = get_mouse_cursor_area(sm.mouse.curr_pos.x, sm.mouse.curr_pos.y);
            log.trace("copying drawn mouse backbuffer to fb at (%d,%d)", area.x, area.y);
            gb_copy_area_to_framebuffer_with_bpp(sm.backbuffer, area, sm.screen.fb_address, sm.screen.pitch, sm.screen.bpp);        
        }
        sm.mouse.needs_redraw = 0;
    }
}

static void brightly_fill_dirty_surfaces() {
    fill_params red = fill_params_solid(0xFFFF0000);
    for (int i = 0; i < sm.dirty_area_count; i++) {
        area a = sm.dirty_areas[i];
        gb_rect(sm.backbuffer, a, a, red, 0);
        if (sm.visual_debug)
            gb_copy_area_to_framebuffer_with_bpp(sm.backbuffer, sm.dirty_areas[i], sm.screen.fb_address, sm.screen.pitch, sm.screen.bpp);        
    }

    if (sm.visual_debug) {
        extern void sdl_present(void);
        sdl_present();
        int deadline = get_timer_ticks() + 100;
        while (get_timer_ticks() < deadline);
    }
}

static void redraw_dirty_surfaces() {
    LOG_TRACE();
    // we go from bottom up...
    dlist_foreach_reverse(&sm.surfaces, surface_t, s) {
        // needs_redraw() is misleading...
        if (!surface_is_visible(s) || !surface_needs_redraw(s))
            continue;

        area surface_dirty_area = surface_get_dirty_area(s);
        area dirty = area_intersect(surface_dirty_area, area_of(0, 0, surface_get_frame(s).width, surface_get_frame(s).height));
        if (area_is_empty(dirty)) {
            surface_mark_clean(s);
            continue;
        }

        // ask the owner of the surface to paint the surface since it's dirty
        gbuffer *buff = surface_get_buffer(s);
        graphics_context_t *gc = new_graphics_context(buff);
        surface_on_paint(s, gc, dirty);

        // merge onto back buffer (ideally, only the clipped region for performance)
        if (surface_is_opaque(s)) {
            gb_copy_area_fast(sm.backbuffer, buff, area_size(surface_dirty_area), point_to_global(area_location(surface_dirty_area), surface_get_frame(s)), area_location(surface_dirty_area));
        } else {
            gb_copy_area_with_alpha(sm.backbuffer, buff, area_size(surface_dirty_area), point_to_global(area_location(surface_dirty_area), surface_get_frame(s)), area_location(surface_dirty_area), 0xFF);
        }

        surface_mark_clean(s);
    }
}

void screen_manager_redraw_screen() {
    LOG_TRACE();
    if (!sm.needs_repaint)
        return;
    
    restore_mouse_cursor_area();

    // 1. Clear dirty regions in backbuffer
    brightly_fill_dirty_surfaces();

    // 2. Draw surfaces bottom → top
    redraw_dirty_surfaces();

    // 3. Draw cursor last
    draw_mouse_cursor();

    // 4. Present, restore cursor
    copy_backbuffer_to_physical_framebuffer();

    sm.needs_repaint = false;
}

// --------------------------------------------------------------

void screen_manager_log_debug_info() {
    log.info("Screen Manager Surfaces");
    dlist_foreach(&sm.surfaces, surface_t, s) {
        surface_log_debug_info(s, "    ");
    }
    sm.visual_debug = !sm.visual_debug;
}