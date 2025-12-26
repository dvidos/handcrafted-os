#include "../memory/string.h"
#include "../graphics/cursors/mouse_cursor.h"
#include "screen_manager.h"
#include "surface.h"
#include "logger.h"

#define MAX_SURFACES       64  // this will be dynamic one day...
#define MAX_DIRTY_AREAS    32  // this one too...


typedef struct mouse_info {
    gpoint curr_pos;         // 0,0 is top left, not screen center
    gsize cursor_size;       // cursor size
    gpoint hotspot_offset;   // 0,0 is top left of the cursor graphic
    gbuffer *pointer;        // masked? alpha? something fast?
    gbuffer *saved_bg;          // saved pixels under cursor
    gpoint saved_bg_origin;     // where the pixels were saved from (maybe mouse moved since)
    gpoint restored_bg_origin;  // where we restored, so we can copy to actual framebuffer
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
    garea dirty_areas[MAX_DIRTY_AREAS];
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
    gb_fill(sm.backbuffer, 0x555555);

    sm.mouse.curr_pos = gpoint_of(sm.width / 2, sm.height / 2);
    sm.mouse.is_visible = 1;
    sm.mouse.cursor_size = gsize_of(32, 32);
    sm.mouse.saved_bg = new_gbuffer(sm.mouse.cursor_size.width, sm.mouse.cursor_size.height);
    sm.mouse.saved_bg_origin = gpoint_of(-1, -1); // signal not captured yet.
    sm.mouse.restored_bg_origin = gpoint_of(-1, -1);

    screen_manager_mark_area_dirty(sm.backbuffer->area); // for first drawing, this should move to desktop composer
}

int screen_manager_add_surface(surface_t *s) {
    if (sm.surface_count >= MAX_SURFACES)
        return 0;
    
    // we could keep them in z-order, to avoid sorting every time?
    sm.surfaces[sm.surface_count++] = s;

    // mark this area as dirty, to redraw it
    screen_manager_mark_area_dirty(garea_of(s->x, s->y, s->w, s->h));

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
    screen_manager_mark_area_dirty(garea_of(s->x, s->y, s->w, s->h));

    return 1;
}

void screen_manager_mark_area_dirty(garea area) {
    // essentially, this is the only way to get to show something on screen!
    // only if marked dirty, will it be asked to be painted, and will end up on screen!

    if (sm.dirty_area_count >= MAX_DIRTY_AREAS) {
        // this should be dynamic but for now, we can do the following, 
        // to force whole screen, even if not performant
        sm.dirty_areas[MAX_DIRTY_AREAS - 1] = garea_with(gpoint_zero(), gsize_of(sm.width, sm.height));
    } else {
        sm.dirty_areas[sm.dirty_area_count++] = area;
    }
    sm.needs_repaint = 1;
}

// --------------------------------------------------------------------------

static inline garea get_mouse_cursor_area(int x, int y) {
    return garea_of(
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
        log.debug("not restoring mouse cursor");
        return; // not captured yet.
    }
    
    log.debug("restoring mouse cursor at (%d,%d)", sm.mouse.saved_bg_origin.x, sm.mouse.saved_bg_origin.y);
    gb_copy_area_fast(sm.backbuffer, sm.mouse.saved_bg, sm.mouse.cursor_size, sm.mouse.saved_bg_origin, gpoint_zero());

    // keep this so we can copy the rectangle from backbuffer to fb
    sm.mouse.restored_bg_origin = sm.mouse.saved_bg_origin;
}

static void draw_mouse_cursor() {
    if (!sm.mouse.is_visible)
        return;
    
    garea mouse_area = get_mouse_cursor_area(sm.mouse.curr_pos.x, sm.mouse.curr_pos.y);
    log.debug("preserving mouse cursor area at (%d,%d,%d,%d)", mouse_area.origin.x, mouse_area.origin.y, mouse_area.size.width, mouse_area.size.height);
    gb_copy_area_fast(sm.mouse.saved_bg, sm.backbuffer, mouse_area.size, gpoint_zero(), mouse_area.origin);
    log.debug("backbuffer pixels: 0x%08x 0x%08x 0x%08x...", sm.backbuffer->buffer_argb[0], sm.backbuffer->buffer_argb[1], sm.backbuffer->buffer_argb[2]);
    log.debug("saved bg   pixels: 0x%08x 0x%08x 0x%08x...", sm.mouse.saved_bg->buffer_argb[0], sm.mouse.saved_bg->buffer_argb[1], sm.mouse.saved_bg->buffer_argb[2]);
    sm.mouse.saved_bg_origin = mouse_area.origin;

    // to see what the saved bg contains
    gb_copy_area_fast(sm.backbuffer, sm.mouse.saved_bg, sm.mouse.cursor_size, gpoint_of(50, 800), gpoint_zero());
    gb_border(sm.backbuffer, garea_of(49, 799, 34, 34), 0, 1, color_tango_red());


    log.debug("drawing mouse cursor at (%d,%d)", mouse_area.origin.x, mouse_area.origin.y);
    gb_draw_cursor32_fast(sm.backbuffer, sm.mouse.curr_pos, &arrow_cursor);
}

// --------------------------------------------------------------------------

static void copy_backbuffer_to_physical_framebuffer() {
    garea area;
    
    // we are only copying what is dirty and has changed
    // otherwise, copying the entire screen is *too* slow
    for (int i = 0; i < sm.dirty_area_count; i++)
        gb_copy_area_to_framebuffer_with_bpp(sm.backbuffer, sm.dirty_areas[i], sm.physical_framebuffer, sm.pitch, sm.bpp);        
    sm.dirty_area_count = 0;

    // we also copy the area of the old and new mouse positions
    if (sm.mouse.needs_redraw) {
        area = garea_with(gpoint_of(sm.mouse.restored_bg_origin.x, sm.mouse.restored_bg_origin.y), sm.mouse.cursor_size);
        log.debug("copying restored mouse backbuffer to fb at (%d,%d)", area.origin.x, area.origin.y);
        gb_copy_area_to_framebuffer_with_bpp(sm.backbuffer, area, sm.physical_framebuffer, sm.pitch, sm.bpp);        

        if (sm.mouse.is_visible) {
            area = get_mouse_cursor_area(sm.mouse.curr_pos.x, sm.mouse.curr_pos.y);
            log.debug("copying drawn mouse backbuffer to fb at (%d,%d)", area.origin.x, area.origin.y);
            gb_copy_area_to_framebuffer_with_bpp(sm.backbuffer, area, sm.physical_framebuffer, sm.pitch, sm.bpp);        
        }
        sm.mouse.needs_redraw = 0;
    }
}

static void redraw_dirty_surfaces() {
    // sort surfaces by z_index
    // for each surface:
    //     if visible:
    //         blit surface buffer → backbuffer
    //         clipped to dirty regions
    for (int i = 0; i < sm.surface_count; i++) {
        surface_t *s = sm.surfaces[i];
        
        if (!s->is_visible) continue;
        if (!s->is_dirty) continue;

        // ask the owner of the surface to paint the surface since it's dirty
        // ideally with a clip region...
        s->draw(s->gbuffer, s->draw_data);
        s->is_dirty = 0;

        // merge onto back buffer (ideally, only the clipped region)
        gb_copy_area_with_alpha(sm.backbuffer, s->gbuffer, s->gbuffer->area.size, gpoint_of(s->x, s->y), gpoint_zero(), 0xFF);
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

