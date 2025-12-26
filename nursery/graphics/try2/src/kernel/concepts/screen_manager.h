#pragma once
#include "../graphics/gbuffer.h"
#include "surface.h"

// screen manager owns the screen. nothing ever draws to screen outside of it.
// window manager must notify screen manager when surfaces are opened, closed, raised, etc.

void initialize_screen_manager(void *framebuffer, int width, int height, int pitch, int bpp);
void screen_manager_redraw_screen();
int screen_manager_add_surface(surface_t *s);
int screen_manager_remove_surface(surface_t *s);
void screen_manager_mark_area_dirty(garea area);

void screen_manager_set_mouse_position(int x, int y);
void screen_manager_get_mouse_position(int *x, int *y);
void screen_manager_set_mouse_visible(int visible);
int  screen_manager_get_mouse_visible();

// FUTURE:
// void screen_manager_take_screenshot(garea area); 


// ------------------------------------------------


// void screen_manager_add_surface(surface_t *);
// void screen_manager_remove_surface(surface_id);
// void screen_manager_set_surface_geometry(surface_id, rect_t);
// void screen_manager_set_surface_z(surface_id, int z);
// void screen_manager_mark_dirty(rect_t);
// void screen_manager_begin_frame(void);
// void screen_manager_draw_surface(surface_t *);
// void screen_manager_end_frame(void);
// void screen_manager_present(void);


// #define MAX_SURFACES 64
// #define MAX_DIRTY_RECTS 32

// typedef struct {
//     int x, y, w, h;
// } rect_t;

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

// typedef struct {
//     // Framebuffer info
//     uint32_t *fb;  // physical framebuffer
//     int width;
//     int height;
//     int pitch;
//     int bpp;
//     uint32_t *backbuffer;
    
//     // Window management
//     surface_t *surfaces[MAX_SURFACES];
//     int surface_count;

//     // Damage tracking
//     rect_t dirty_rects[MAX_DIRTY_RECTS];
//     int dirty_count;

//     // mouse cursor state
//     int cursor_x;
//     int cursor_y;
//     int cursor_w;
//     int cursor_h;
//     uint32_t *cursor_bitmap;
//     uint32_t cursor_bg[64 * 64];  // saved pixels under cursor
//     bool cursor_visible;

//     // State flags
//     bool needs_repaint;
// } screen_manager_t;

// void screen_manager_initialize(void *framebuffer, int width, int height, int pitch, int bpp) {

// }

// void screen_manager_update_screen() {
//     if (!sm->needs_repaint)
//         return;

//     // 1. Clear dirty regions in backbuffer
//     for each dirty_rect:
//         fill backbuffer region with background

//     // 2. Draw surfaces bottom → top
//     sort surfaces by z_index
//     for each surface:
//         if visible:
//             blit surface buffer → backbuffer
//             clipped to dirty regions

//     // 3. Draw cursor last
//     if cursor_visible:
//         save pixels under cursor
//         blit cursor bitmap → backbuffer

//     // 4. Present
//     memcpy(sm->fb, sm->backbuffer, screen_size);

//     sm->dirty_count = 0;
//     sm->needs_repaint = false;
// }

