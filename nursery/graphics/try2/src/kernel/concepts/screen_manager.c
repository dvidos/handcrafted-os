#include "../memory/string.h"
#include "screen_manager.h"
#include "surface.h"

#define MAX_SURFACES   64  // this will be dynamic one day...


typedef struct mouse_info {
    int x;
    int y;
    int cursor_size; // 32 for 32x32 square
    int hotspot_x_offset;   // within the 32x32 graphic
    int hotspot_y_offset;
    gbuffer *pointer;
    gbuffer *background;  // saved pixels under cursor
    int is_visible;
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

    sm.mouse.x = (sm.width / 2);
    sm.mouse.y = (sm.height / 2);
    sm.mouse.is_visible = 1;
    sm.mouse.cursor_size = 32;
    sm.mouse.background = new_gbuffer(sm.mouse.cursor_size, sm.mouse.cursor_size);
}

int screen_manager_add_surface(surface_t *s) {
    if (sm.surface_count >= MAX_SURFACES)
        return 0;
    
    sm.surfaces[sm.surface_count++] = s;
    // we could keep them in z-order, to avoid sorting every time?
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
    return 0;
}

// --------------------------------------------------------------------------

void screen_manager_set_mouse_position(int x, int y) {
    sm.mouse.x = x;
    sm.mouse.y = y;
    sm.needs_repaint = 1;
}

void screen_manager_get_mouse_position(int *x, int *y) {
    *x = sm.mouse.x;
    *y = sm.mouse.y;
}

void screen_manager_set_mouse_visible(int visible) {
    sm.mouse.is_visible = visible;
    sm.needs_repaint = 1;
}

int screen_manager_get_mouse_visible() {
    return sm.mouse.is_visible;
}

static void draw_mouse_cursor() {
    if (!sm.mouse.is_visible)
        return;
    
    gb_copy_area_fast(sm.mouse.background, sm.backbuffer, gsize_of(sm.mouse.cursor_size, sm.mouse.cursor_size), gpoint_zero(), 
        gpoint_of(sm.mouse.x - sm.mouse.hotspot_x_offset, sm.mouse.y - sm.mouse.hotspot_y_offset));

    color mouse_color = color_tango_red();
    gb_paint_pixel(sm.backbuffer, gpoint_of(sm.mouse.x - 1, sm.mouse.y), mouse_color);
    gb_paint_pixel(sm.backbuffer, gpoint_of(sm.mouse.x + 1, sm.mouse.y), mouse_color);
    gb_paint_pixel(sm.backbuffer, gpoint_of(sm.mouse.x, sm.mouse.y - 1), mouse_color);
    gb_paint_pixel(sm.backbuffer, gpoint_of(sm.mouse.x, sm.mouse.y + 1), mouse_color);

    gb_border(sm.backbuffer, garea_with(
        gpoint_of(sm.mouse.x - sm.mouse.hotspot_x_offset, sm.mouse.y - sm.mouse.hotspot_y_offset),
        gsize_of(sm.mouse.cursor_size, sm.mouse.cursor_size)
    ), 0, 1, mouse_color);
}

static void restore_mouse_cursor_area() {
    if (!sm.mouse.is_visible)
        return;
    
    gb_copy_area_fast(sm.backbuffer, sm.mouse.background, gsize_of(sm.mouse.cursor_size, sm.mouse.cursor_size), gpoint_zero(), 
        gpoint_of(sm.mouse.x - sm.mouse.hotspot_x_offset, sm.mouse.y - sm.mouse.hotspot_y_offset));
}

// --------------------------------------------------------------------------

static void copy_backbuffer_to_physical_framebuffer() {
    // we need to convert formats.
    // our buffers are AARRGGBB, our VBE framebuffer is RRGGBB.
    uint8_t *vbe = sm.physical_framebuffer;
    uint32_t *buff_argb = sm.backbuffer->buffer_argb;
    int count = sm.height * sm.pitch;

    // this is too slow, we need to track and only paint dirty areas (e.g. mouse)

    if (sm.bpp == 32) {
        memcpy(vbe, buff_argb, count);
    } else if (sm.bpp == 24) {
        while (count-- > 0) {
            // alpha channel is lost here.
            *vbe++ = color_b(*buff_argb);
            *vbe++ = color_g(*buff_argb);
            *vbe++ = color_r(*buff_argb);
            buff_argb++;
        }
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
        s->draw(s->gbuffer, s->draw_data);
        s->is_dirty = 0;

        // merge onto back buffer (but it says clipped to dirty regions... hmm)
        gb_copy_area_with_alpha(sm.backbuffer, s->gbuffer, s->gbuffer->area.size, gpoint_of(s->x, s->y), gpoint_zero(), 0xFF);
    }
}


void screen_manager_redraw_screen() {
    if (!sm.needs_repaint)
        return;
    
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
    restore_mouse_cursor_area();

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

