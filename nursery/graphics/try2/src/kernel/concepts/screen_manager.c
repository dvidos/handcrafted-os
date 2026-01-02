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

#define MAX_SURFACES          64  // this will be dynamic one day...
#define KBD_FOCUS_STACK_SIZE  16  // stack of modal focused surfaces
#define MAX_DIRTY_AREAS       32  // this one too...


typedef struct mouse_info {
    point curr_pos;         // 0,0 is top left, not screen center
    size cursor_size;       // cursor size
    point hotspot_offset;   // 0,0 is top left of the cursor graphic
    gbuffer *pointer;        // masked? alpha? something fast?
    gbuffer *saved_bg;          // saved pixels under cursor
    point saved_bg_origin;     // where the pixels were saved from (maybe mouse moved since)
    point restored_bg_origin;  // where we restored, so we can copy to actual framebuffer
    int is_visible : 1;
    int needs_redraw : 1;
} mouse_info_t;

typedef struct screen_manager {
    // Framebuffer info
    void *physical_framebuffer;  // physical framebuffer
    int width;
    int height;
    int pitch;
    int bpp;

    // what we draw to
    gbuffer *backbuffer;
    mouse_info_t mouse;

    // dirty areas, only these are painted, for performance
    int needs_repaint;
    area dirty_areas[MAX_DIRTY_AREAS];
    int dirty_area_count;

    // event distribution:
    surface_t *surface_list_head;  // z-order top
    surface_t *surface_list_tail;
    surface_t *mouse_capture; // nullable
    surface_t *keyboard_focus_stack[KBD_FOCUS_STACK_SIZE]; // nullable
    int keyboard_focus_stack_count;

} screen_manager_t;
static screen_manager_t sm;

void initialize_screen_manager(void *framebuffer, int width, int height, int pitch, int bpp) {
    memset(&sm, 0, sizeof(screen_manager_t));

    sm.physical_framebuffer = framebuffer;
    sm.width = width;
    sm.height = height;
    sm.pitch = pitch;
    sm.bpp = bpp;

    sm.backbuffer = new_gbuffer(width, height);
    gb_fill(sm.backbuffer, 0x3f9fbf);

    sm.mouse.curr_pos = point_of(sm.width / 2, sm.height / 2);
    sm.mouse.is_visible = 1;
    sm.mouse.cursor_size = size_of(32, 32);
    sm.mouse.saved_bg = new_gbuffer(sm.mouse.cursor_size.width, sm.mouse.cursor_size.height);
    sm.mouse.saved_bg_origin = point_of(-1, -1); // signal not captured yet.
    sm.mouse.restored_bg_origin = point_of(-1, -1);

    screen_manager_mark_area_dirty(sm.backbuffer->area); // for first drawing, this should move to desktop composer
}

// -----------------------------------------------------------------------

static void _surface_invalidated(void *surface, area dirty) {
    screen_manager_mark_area_dirty(dirty);
}

static surface_owner_interface_t surfaces_interface = {
    .surface_invalidated = _surface_invalidated,
};

// -----------------------------------------------------------------------

size screen_manager_get_screen_size() {
    return size_of(sm.width, sm.height);
}

area screen_manager_get_screen_area() {
    return area_of(0, 0, sm.width, sm.height);
}

int screen_manager_add_surface(surface_t *s) {
    DLL_PUSH_HEAD(sm.surface_list_head, sm.surface_list_tail, s);

    // need to redraw this area
    s->owner_interface = &surfaces_interface;
    screen_manager_mark_area_dirty(s->frame);
    return 1;
}

int screen_manager_remove_surface(surface_t *s) {
    DLL_REMOVE(sm.surface_list_head, sm.surface_list_tail, s);

    // need to redraw this area
    screen_manager_mark_area_dirty(s->frame);
    s->owner_interface = NULL;
    return 1;
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

void screen_manager_bring_surface_to_top(surface_t *s) {
    DLL_REMOVE(sm.surface_list_head, sm.surface_list_tail, s);
    DLL_PUSH_HEAD(sm.surface_list_head, sm.surface_list_tail, s);
}

void screen_manager_push_surface_to_bottm(surface_t *s) {
    DLL_REMOVE(sm.surface_list_head, sm.surface_list_tail, s);
    DLL_PUSH_TAIL(sm.surface_list_head, sm.surface_list_tail, s);
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

void screen_manager_set_mouse_position(int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > sm.width - 1) x = sm.width - 1;
    if (y > sm.height - 1) y = sm.height - 1;
    
    if (x == sm.mouse.curr_pos.x && y == sm.mouse.curr_pos.y)
        return;
    
    // mark old and new areas as dirty, to repaint them.
    sm.mouse.curr_pos.x = x;
    sm.mouse.curr_pos.y = y;
    sm.mouse.needs_redraw = 1; // we don't mark dirty, as _we_ handle the bg buffer for this
    sm.needs_repaint = true; 
}

void screen_manager_get_mouse_position(int *x, int *y) {
    *x = sm.mouse.curr_pos.x;
    *y = sm.mouse.curr_pos.y;
}

void screen_manager_set_mouse_visible(int visible) {
    sm.mouse.is_visible = visible;
    sm.mouse.needs_redraw = 1;
    sm.needs_repaint = true; 
}

int screen_manager_get_mouse_visible() {
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
        gb_copy_area_to_framebuffer_with_bpp(sm.backbuffer, sm.dirty_areas[i], sm.physical_framebuffer, sm.pitch, sm.bpp);        
    sm.dirty_area_count = 0;

    // we also copy the area of the old and new mouse positions
    if (sm.mouse.needs_redraw) {
        area = area_with(point_of(sm.mouse.restored_bg_origin.x, sm.mouse.restored_bg_origin.y), sm.mouse.cursor_size);
        log.trace("copying restored mouse backbuffer to fb at (%d,%d)", area.x, area.y);
        gb_copy_area_to_framebuffer_with_bpp(sm.backbuffer, area, sm.physical_framebuffer, sm.pitch, sm.bpp);        

        if (sm.mouse.is_visible) {
            area = get_mouse_cursor_area(sm.mouse.curr_pos.x, sm.mouse.curr_pos.y);
            log.trace("copying drawn mouse backbuffer to fb at (%d,%d)", area.x, area.y);
            gb_copy_area_to_framebuffer_with_bpp(sm.backbuffer, area, sm.physical_framebuffer, sm.pitch, sm.bpp);        
        }
        sm.mouse.needs_redraw = 0;
    }
}

static void blacken_dirty_surfaces() {
    color_params black = color_params_solid(color_black());
    for (int i = 0; i < sm.dirty_area_count; i++) {
        area a = sm.dirty_areas[i];
        gb_rect(sm.backbuffer, a, a, black, 0);
    }
}

static void redraw_dirty_surfaces() {
    LOG_TRACE();
    for (surface_t *s = sm.surface_list_head; s != 0; s = s->next) {
        if (!s->is_visible || !s->needs_redraw)
            continue;

        area dirty = area_intersect(s->dirty_area, area_of(0, 0, s->frame.width, s->frame.height));
        if (area_is_empty(dirty)) {
            s->dirty_area = area_zero();
            s->needs_redraw = false;
            continue;
        }

        // ask the owner of the surface to paint the surface since it's dirty
        graphics_context_t *gc = new_graphics_context(s->buffer);
        surface_begin_draw(s, gc);
        s->callbacks.paint(s, gc, s->dirty_area);
        surface_end_draw(s);

        // merge onto back buffer (ideally, only the clipped region for performance)
        if (s->is_opaque) {
            // log.debug("dirty area is (%d,%d,%d,%d)", s->dirty_area.x, s->dirty_area.y, s->dirty_area.width, s->dirty_area.height);
            gb_copy_area_fast(sm.backbuffer, s->buffer, area_size(s->dirty_area), point_to_global(area_location(s->dirty_area), s->frame), area_location(s->dirty_area));
        } else {
            gb_copy_area_with_alpha(sm.backbuffer, s->buffer, area_size(s->dirty_area), point_to_global(area_location(s->dirty_area), s->frame), area_location(s->dirty_area), 0xFF);
        }

        surface_mark_clean(s);
    }
}

void screen_manager_redraw_screen() {
    if (!sm.needs_repaint)
        return;
    
    restore_mouse_cursor_area();

    // 1. Clear dirty regions in backbuffer
    blacken_dirty_surfaces();

    // 2. Draw surfaces bottom → top
    redraw_dirty_surfaces();

    // 3. Draw cursor last
    draw_mouse_cursor();

    // 4. Present, restore cursor
    copy_backbuffer_to_physical_framebuffer();

    sm.needs_repaint = false;
}

// --------------------------------------------------------------------------

void screen_manager_push_keyboard_focus(surface_t *s) {
    if (sm.keyboard_focus_stack_count >= KBD_FOCUS_STACK_SIZE) {
        log.error("key_capture push request, while stack full, failing this request");
        return;
    }

    sm.keyboard_focus_stack[sm.keyboard_focus_stack_count++] = s;
    if (s->callbacks.on_focus_gained)
        s->callbacks.on_focus_gained(s);
}

void screen_manager_pop_keyboard_focus() {
    if (sm.keyboard_focus_stack_count < 1) {
        log.warn("key_capture pop requested with no surfaces on focus stack");
        return;
    }
    surface_t *s = sm.keyboard_focus_stack[sm.keyboard_focus_stack_count - 1];
    if (s->callbacks.on_focus_lost)
        s->callbacks.on_focus_lost(s);
    
    sm.keyboard_focus_stack[sm.keyboard_focus_stack_count - 1] = 0;
    sm.keyboard_focus_stack_count--;
}

void screen_manager_set_keyboard_focus(surface_t *s) {
    if (sm.keyboard_focus_stack_count > 1)
        log.warn("key_capture set, while %d surfaces on stack", sm.keyboard_focus_stack_count);
    
    if (sm.keyboard_focus_stack_count > 0) {
        // if something is there, at least unfocus it.
        surface_t *old_s = sm.keyboard_focus_stack[sm.keyboard_focus_stack_count - 1];
        if (old_s->callbacks.on_focus_lost)
            old_s->callbacks.on_focus_lost(old_s);
    } else if (sm.keyboard_focus_stack_count == 0) {
        sm.keyboard_focus_stack_count = 1;
    }
    
    sm.keyboard_focus_stack[sm.keyboard_focus_stack_count - 1] = s;
    if (s->callbacks.on_focus_gained)
        s->callbacks.on_focus_gained(s);
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

// --------------------------------------------------------------

void screen_manager_dispatch_key_event(key_event_t e) {
    if (sm.keyboard_focus_stack_count == 0) {
        // log.debug("No surface found to handle keyboard event");
        return;
    }

    surface_t *focused = sm.keyboard_focus_stack[sm.keyboard_focus_stack_count - 1];
    surface_handle_key(focused, e);
}

void screen_manager_dispatch_mouse_event(mouse_event_t e) {
    surface_t *target = 0;

    if (sm.mouse_capture) {
        target = sm.mouse_capture;
    } else {
        // perform hit test, top to bottom
        for (surface_t *s = sm.surface_list_head; s != 0; s = s->next) {
            if (!area_contains(s->frame, e.pos))
                continue;
            if (!s->is_visible || !s->accepts_mouse) 
                continue;

            target = s;
            break;
        }
    }

    if (!target)
        return;

    target->callbacks.on_mouse_event(target, mouse_event_localized(e, target->frame));
}
