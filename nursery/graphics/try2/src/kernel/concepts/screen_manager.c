#include "../memory/string.h"
#include "../graphics/cursors/mouse_cursor.h"
#include "screen_manager.h"
#include "../app_kit/surface.h"
#include "../app_kit/graphics_context.h"
#include "logger.h"
#include "../algorithms/rand.h"

#define MAX_SURFACES       64  // this will be dynamic one day...
#define MAX_DIRTY_AREAS    32  // this one too...


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

typedef struct {
    // Framebuffer info
    void *physical_framebuffer;  // physical framebuffer
    int width;
    int height;
    int pitch;
    int bpp;

    // what we draw to
    gbuffer *backbuffer;
    mouse_info_t mouse;

    // surfaces management - for composing the picture
    surface_t *surfaces[MAX_SURFACES];
    int surface_count;

    // dirty areas, only these are painted, for performance
    area dirty_areas[MAX_DIRTY_AREAS];
    int dirty_area_count;

    // to avoid useless processing
    int needs_repaint;
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
    uint32_t seed = 123;
    for (int i = 0; i < 10; i++) {
        color c = 0xFF000000 | (rand_r(&seed) & 0xFFFFFF);
        area a = area_of(rand_r(&seed) % width, rand_r(&seed) % height, rand_r(&seed) % 1000, rand_r(&seed) % 700);
        gb_rect(sm.backbuffer, a, sm.backbuffer->area, color_params_solid(c), 0);
    }

    sm.mouse.curr_pos = point_of(sm.width / 2, sm.height / 2);
    sm.mouse.is_visible = 1;
    sm.mouse.cursor_size = size_of(32, 32);
    sm.mouse.saved_bg = new_gbuffer(sm.mouse.cursor_size.width, sm.mouse.cursor_size.height);
    sm.mouse.saved_bg_origin = point_of(-1, -1); // signal not captured yet.
    sm.mouse.restored_bg_origin = point_of(-1, -1);

    screen_manager_mark_area_dirty(sm.backbuffer->area); // for first drawing, this should move to desktop composer
}

int screen_manager_add_surface(surface_t *s) {
    if (sm.surface_count >= MAX_SURFACES)
        return 0;
    
    // we could keep them in z-order, to avoid sorting every time?
    sm.surfaces[sm.surface_count++] = s;

    // mark this area as dirty, to redraw it
    screen_manager_mark_area_dirty(s->frame);

    return 1;
}

int screen_manager_remove_surface(surface_t *s) {
    int index = -1;
    for (int i = 0; i < MAX_SURFACES; i++) {
        if (sm.surfaces[i] == s) {
            index = i;
            break;
        }
    }
    if (index == -1)
        return 0;
    
    // move all the other items one down
    for (int i = index; i < MAX_SURFACES - 1; i++)
        sm.surfaces[i] = sm.surfaces[i + 1];
    sm.surface_count--;

    // mark this area as dirty, to redraw it
    screen_manager_mark_area_dirty(s->frame);

    return 1;
}

void screen_manager_mark_area_dirty(area area) {
    // essentially, this is the only way to get to show something on screen!
    // only if marked dirty, will it be asked to be painted, and will end up on screen!

    if (sm.dirty_area_count >= MAX_DIRTY_AREAS) {
        // this should be dynamic but for now, we can do the following, 
        // to force whole screen, even if not performant
        sm.dirty_areas[MAX_DIRTY_AREAS - 1] = area_with(point_zero(), size_of(sm.width, sm.height));
    } else {
        sm.dirty_areas[sm.dirty_area_count++] = area;
    }
    sm.needs_repaint = 1;
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
    sm.needs_repaint = 1; 
}

void screen_manager_get_mouse_position(int *x, int *y) {
    *x = sm.mouse.curr_pos.x;
    *y = sm.mouse.curr_pos.y;
}

void screen_manager_set_mouse_visible(int visible) {
    sm.mouse.is_visible = visible;
    sm.mouse.needs_redraw = 1;
    sm.needs_repaint = 1; 
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
    area = area_of(5, 5, 32, 32);
    gb_copy_area_fast(sm.backbuffer, sm.mouse.saved_bg, area_size(area), area_location(area), point_zero());
    gb_border(sm.backbuffer, area, area, 0, 1, 0xFFFFFFFF);
    screen_manager_mark_area_dirty(area);

    
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

static void redraw_dirty_surfaces() {
    for (int i = 0; i < sm.surface_count; i++) {
        surface_t *s = sm.surfaces[i];
        if (!s->is_visible || !s->needs_redraw)
            continue;
        
        area dirty = area_intersect(s->dirty_area, area_of(0, 0, s->frame.width, s->frame.height));
        if (area_is_empty(dirty)) {
            s->dirty_area = area_zero();
            s->needs_redraw = false;
            continue;
        }

        // ask the owner of the surface to paint the surface since it's dirty
        graphics_context_t *ctx = new_graphics_context(s->buffer);
        surface_begin_draw(s, ctx);
        s->paint(s, ctx, s->dirty_area);
        surface_end_draw(s);

        // merge onto back buffer (ideally, only the clipped region for performance)
        if (s->is_opaque) {
            gb_copy_area_fast(sm.backbuffer, s->buffer, area_size(s->dirty_area), point_to_global(area_location(s->dirty_area), s->frame), area_location(s->dirty_area));
        } else {
            gb_copy_area_with_alpha(sm.backbuffer, s->buffer, area_size(s->dirty_area), point_to_global(area_location(s->dirty_area), s->frame), area_location(s->dirty_area), 0xFF);
        }
        s->dirty_area = area_zero();
        s->needs_redraw = false;
    }
}

void screen_manager_redraw_screen() {
    if (!sm.needs_repaint)
        return;
    
    restore_mouse_cursor_area();

    // ...
    // 1. Clear dirty regions in backbuffer
    // for each dirty_rect:
    //     fill backbuffer region with background

    // 2. Draw surfaces bottom → top
    redraw_dirty_surfaces();

    // 3. Draw cursor last
    draw_mouse_cursor();

    // 4. Present, restore cursor
    copy_backbuffer_to_physical_framebuffer();


    sm.needs_repaint = 0;
}

// ----------------------------------------------



// void screen_manager_add_surface(surface_t *);
// void screen_manager_remove_surface(surface_id);
// void screen_manager_set_surface_geometry(surface_id, rect_t);
// void screen_manager_set_surface_z(surface_id, int z);
// void screen_manager_mark_dirty(rect_t);
// void screen_manager_begin_frame(void);
// void screen_manager_draw_surface(surface_t *);
// void screen_manager_end_frame(void);
// void screen_manager_present(void);

// // add position, z-index, flags, to compose a whole screen
// typedef struct surface {
//     int x, y;
//     int width, height;
//     gbuffer *gbuffer;
//     bool visible;
//     uint32_t flags;     // desktop, popup, always_on_top, etc.
//     int z_index;
//     rect_t dirty;       // surface-local dirty region
//     int is_dirty;
// } surface_t;

