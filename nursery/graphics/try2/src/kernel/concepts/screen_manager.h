#pragma once
#include "../graphics/gbuffer.h"
#include "../app_kit/surface.h"

// screen manager owns the screen. nothing ever draws to screen outside of it.
// window manager must notify screen manager when surfaces are opened, closed, raised, etc.
// dock, menu bar, alt-tab switcher, all these are surfaces, not windows.

void initialize_screen_manager(void *framebuffer, int width, int height, int pitch, int bpp);

size screen_manager_get_screen_size();
void screen_manager_redraw_screen();
int  screen_manager_add_surface(surface_t *s);
int  screen_manager_remove_surface(surface_t *s);
void screen_manager_mark_area_dirty(area area);
void screen_manager_bring_surface_to_top(surface_t *s);
void screen_manager_push_surface_to_bottm(surface_t *s);

// mouse pointer management
void screen_manager_set_mouse_position(int x, int y);
void screen_manager_get_mouse_position(int *x, int *y);
void screen_manager_set_mouse_visible(int visible);
int  screen_manager_get_mouse_visible();

// FUTURE:
// void screen_manager_take_screenshot(garea area); 
// void screen_manager_dim_to_screen_saver(); 

// ------------------------------------------------

void screen_manager_push_keyboard_focus(surface_t *s);
void screen_manager_pop_keyboard_focus();
void screen_manager_set_keyboard_focus(surface_t *s);
void screen_manager_set_mouse_capture(surface_t *s);
void screen_manager_clear_mouse_capture();

// ------------------------------------------------

void screen_manager_dispatch_key_event(key_event_t e);
void screen_manager_dispatch_mouse_event(mouse_event_t e);

// ------------------------------------------------

