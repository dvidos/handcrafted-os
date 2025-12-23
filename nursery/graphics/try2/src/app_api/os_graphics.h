#pragma once
#include <stdint.h>

/*
    So, core layers:
    - app kit surface: windows, views, buttons, etc. 
    - graphics context: resolution-independent drawing of app kit or views, on graphics buffers
    - window manager: organizes owned windows and decides what is drawn where on screen
    
    An app will know:
    - mostly app kit (it holds one surface)
    - it can use special graphics view, to gain access to arbitrary drawing (e.g. maps, charts, images etc)
    - it will use float coordinates, where 1 = 1pt = 1/72 inch ~= 0.3 mm

    Alternatively, the graphics context only converts the function calls
    into commands and queues them for an external rasterizer to render them,
    which would be managed by the WM. 
    This gains flexibility with the backend, allows zooming, scrolling etc. 
    The graphics context and commands will still be resolution-independent.

*/

// --------------------------------------------------------------

typedef struct app_kit_widget app_kit_widget;

struct app_kit_widget {
    float x, y, w, h;
    void (*draw)(app_kit_widget *wgt, graphics_context *ctx);
};

// or each discrete widget exposes a `get_widget_interface()` to return an interface
app_kit_widget *create_app_kit_button(char *text, float x, float y);
app_kit_widget *create_app_kit_label(char *text, float x, float y);
app_kit_widget *create_app_kit_textbox(float x, float y, float w, float h);
app_kit_widget *create_app_kit_inputbox(float x, float y, float w, float h);
app_kit_widget *create_app_kit_image_view(float x, float y, float w, float h);
app_kit_widget *create_app_kit_canvas(float x, float y, float w, float h);
app_kit_widget *create_app_kit_graph(float x, float y, float w, float h);
app_kit_widget *create_app_kit_scroll_view(float x, float y, float w, float h);
app_kit_widget *create_app_kit_console_view(float x, float y, float w, float h);

// --------------------------------------------------------------

// graphics context is more primitive graphic actions, still resolution-independent
// they could be rendered on screen, in a PDF, in an image file, on printer etc.
// in our simple design, it scales and draws on graphics_bitmap directly, 
// on a more elaborate design, it would create a list of commands, and
// a separate rasterizer would play the commands against a graphics_bitmap.

typedef struct paint_info paint_info;
typedef struct font_info font_info;
typedef struct gbuffer gbuffer;

typedef struct graphics_context {
    int width, height;
    float transformation_matrix[6]; // scale and rotation
    paint_info *current_fill;
    paint_info *current_stroke;
    font_info *current_font;
    float clip_x, clip_y, clip_w, clip_h;
    gbuffer *graphics_buffer; // we'll see
} graphics_context;

graphics_context *create_graphics_context();
void gc_destroy_graphics_context(graphics_context *ctx);

void gc_push_state(graphics_context *ctx); // make it easy for temporarily setting some params, drawing...
void gc_pop_state(graphics_context *ctx);  // ...and then going back to whatever they were before.
void gc_set_fill(graphics_context *ctx, paint_info *fill);
void gc_set_stroke(graphics_context *ctx, paint_info *stroke);
void gc_set_font(graphics_context *ctx, font_info *font);
void gc_trans_move(graphics_context *ctx, float dx, float dy);
void gc_trans_scale(graphics_context *ctx, float sx, float sy);
void gc_trans_rotate(graphics_context *ctx, float angle_in_radians);
void gc_fill_rect(graphics_context *ctx, float x, float y, float w, float h);
void gc_draw_line(graphics_context *ctx, float x1, float y1, float x2, float y2);
void gc_draw_text(graphics_context *ctx, const char *text, float x, float base_y);

// --------------------------------------------------------------

// window manager holds the stack of all windows,
// can paint the complete picture, can distribute events to focused window
// it's akin to the console manager we have in our handcrafted-os

typedef struct event event;
typedef struct wm_window {
    int id;
    float x, y, w, h;
    graphics_context *ctx;
    int is_visible: 1;
    int is_full_screen: 1;
} wm_window;

wm_window *create_wm_window(graphics_context *ctx);
void destroy_wm_window(wm_window *w);
void wm_raise_window(wm_window *w);
void wm_lower_window(wm_window *w);
void wm_hide_window(wm_window *w);
void wm_show_window(wm_window *w);
void wm_make_window_full_screen(wm_window *w);
void wm_unmake_window_full_screen(wm_window *w);
// from OS
void wm_repaint_screen(); // repaints dirty areas only
void wm_dispatch_event(event *e); // to current window

// --------------------------------------------------------------
