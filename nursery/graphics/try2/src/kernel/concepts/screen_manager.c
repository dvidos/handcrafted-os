#include "../memory/string.h"
#include "screen_manager.h"
#include "surface.h"

#define MAX_SURFACES       64  // this will be dynamic one day...
#define MAX_DIRTY_AREAS    32  // this one too...


typedef struct mouse_info {
    gpoint curr_pos;        // 0,0 is top left, not screen center
    gsize cursor_size;      // cursor size
    gpoint hotspot_offset;   // 0,0 is top left of the cursor graphic
    gbuffer *pointer;       // masked? alpha? something fast?
    gbuffer *bg;            // saved pixels under cursor
    gpoint bg_position;     // where the pixels were saved from (maybe mouse moved since)
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
    sm.mouse.bg = new_gbuffer(sm.mouse.cursor_size.width, sm.mouse.cursor_size.height);

    screen_manager_mark_dirty(sm.backbuffer->area); // for first drawing, this should move to desktop composer
}

int screen_manager_add_surface(surface_t *s) {
    if (sm.surface_count >= MAX_SURFACES)
        return 0;
    
    // we could keep them in z-order, to avoid sorting every time?
    sm.surfaces[sm.surface_count++] = s;

    // mark this area as dirty, to redraw it
    screen_manager_mark_dirty(garea_of(s->x, s->y, s->w, s->h));

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
    screen_manager_mark_dirty(garea_of(s->x, s->y, s->w, s->h));

    return 1;
}

void screen_manager_mark_dirty(garea area) {
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

static void restore_mouse_cursor() {
    gb_copy_area_fast(sm.backbuffer, sm.mouse.bg, sm.mouse.cursor_size, sm.mouse.bg_position, gpoint_zero());
}

static void draw_mouse_cursor() {
    if (!sm.mouse.is_visible)
        return;
    
    garea mouse_area = get_mouse_cursor_area(sm.mouse.curr_pos.x, sm.mouse.curr_pos.y);
    gb_copy_area_fast(sm.mouse.bg, sm.backbuffer, mouse_area.size, gpoint_zero(), mouse_area.origin);
    sm.mouse.bg_position = mouse_area.origin;
    
    color mouse_color = color_tango_red();
    gb_paint_pixel(sm.backbuffer, gpoint_of(sm.mouse.curr_pos.x - 1, sm.mouse.curr_pos.y), mouse_color);
    gb_paint_pixel(sm.backbuffer, gpoint_of(sm.mouse.curr_pos.x + 1, sm.mouse.curr_pos.y), mouse_color);
    gb_paint_pixel(sm.backbuffer, gpoint_of(sm.mouse.curr_pos.x, sm.mouse.curr_pos.y - 1), mouse_color);
    gb_paint_pixel(sm.backbuffer, gpoint_of(sm.mouse.curr_pos.x, sm.mouse.curr_pos.y + 1), mouse_color);
    gb_border(sm.backbuffer, mouse_area, 0, 1, mouse_color); // temp
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
        area = get_mouse_cursor_area(sm.mouse.bg_position.x, sm.mouse.bg_position.y);
        gb_copy_area_to_framebuffer_with_bpp(sm.backbuffer, area, sm.physical_framebuffer, sm.pitch, sm.bpp);        

        if (sm.mouse.is_visible) {
            area = get_mouse_cursor_area(sm.mouse.curr_pos.x, sm.mouse.curr_pos.y);
            gb_copy_area_to_framebuffer_with_bpp(sm.backbuffer, area, sm.physical_framebuffer, sm.pitch, sm.bpp);        
        }
        sm.mouse.needs_redraw = 0;
    }
}

static void draw_surfaces() {
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
    
    restore_mouse_cursor();

    // ...
    // 1. Clear dirty regions in backbuffer
    // for each dirty_rect:
    //     fill backbuffer region with background

    // 2. Draw surfaces bottom → top
    draw_surfaces();

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

