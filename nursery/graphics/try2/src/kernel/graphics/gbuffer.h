#pragma once
#include "../fundamentals.h"
#include "../memory/malloc.h"
#include "../memory/string.h"
#include "color.h"
#include "fonts/font8x16.h"
#include "cursors/mouse_cursor.h"
#include "icons/icon32.h"
#include "geometry.h"


typedef struct gbuffer {
    area area; // origin is always (0,0), to allow operations with nested buffers
    int buffer_size;
    uint32_t *buffer_argb;
} gbuffer;

// ------------------------------------------------------------------------

typedef enum color_fill_type {
    FILL_TYPE_SOLID,
    FILL_TYPE_LINEAR_GRADIENT,
} color_fill_type;

typedef struct color_params {
    color_fill_type fill_type;
    color clr;
    color clr2;
    point gradient_p1;
    point gradient_p2;
    ease_function *ease;
    vector gradient_v;
    float gradient_len_sq;
} color_params;

static inline color_params color_params_solid(color clr) { return (color_params){.fill_type = FILL_TYPE_SOLID, .clr = clr}; };
static inline color_params color_params_gradient(color c1, color c2, point p1, point p2, ease_function ease) { return (color_params){
    .fill_type = FILL_TYPE_LINEAR_GRADIENT, .clr = c1, .clr2 = c2, .gradient_p1 = p1, .gradient_p2 = p2, .ease = ease,
    .gradient_v = vector_from_to(p1, p2), 
    .gradient_len_sq = vector_dot_product(vector_from_to(p1, p2), vector_from_to(p1, p2))
}; }

// ------------------------------------------------------------------------

typedef struct shadow_params {
    color clr;
    uint8_t opacity;
    int offset_x;
    int offset_y;
    int blur_radius;
} shadow_params;

static inline shadow_params shadow_params_of(color clr, uint8_t opacity, int offset_x, int offset_y, int blur_radius) { return (shadow_params){.clr = clr, .opacity = opacity, .offset_x = offset_x, .offset_y = offset_y, .blur_radius = blur_radius}; }

// ------------------------------------------------------------------------

typedef struct text_params {
    font8x16 *font;
    // could have variances like kerning, size, etc
    alignment align;
} text_params;

static inline text_params text_params_of(font8x16 *font, alignment align) { return (text_params){.font = font, .align = align}; }

// ------------------------------------------------------------------------

void initialize_gbuffer(gbuffer *aux_buffer); // called from graphics initialization code

gbuffer *new_gbuffer(int width, int height);
void gb_free(gbuffer *gb);
void gb_clear(gbuffer *gb);
color gb_get_pixel(gbuffer *gb, point p);
void gb_paint_pixel(gbuffer *gb, point p, color clr);
void gb_fill(gbuffer *gb, color clr);
void gb_rect(gbuffer *gb, area rect, area clip, color_params clr_prm, int radius);
void gb_border(gbuffer *gb, area rect, area clip, int radius, int border_width, color clr);
void gb_blur(gbuffer *gb, area rect, int radius, int do_blur_alpha);
void gb_text(gbuffer *gb, area rect, area clip, const char *text, text_params params, color clr);
void gb_text_demo(gbuffer *gb, area rect, font8x16 *font, color clr);
void gb_icon(gbuffer *gb, area rect, area clip, const icon32 *icon, color clr);
void gb_draw_cursor32_fast(gbuffer *gb, point mouse_pos, const cursor32 *cursor);

void gb_copy_area_fast(gbuffer *dest, gbuffer *src, size size, point dest_origin, point src_origin);
void gb_copy_area_with_alpha(gbuffer *dest, gbuffer *src, size size, point dest_origin, point src_origin, uint8_t global_alpha);
void gb_copy_area_scaled(gbuffer *gb, ...);
void gb_copy_area_to_framebuffer_with_bpp(gbuffer *gb, area area, void *dest_buffer, int dest_pitch, int dest_bpp);
void gb_drop_shadow(gbuffer *gb, const gbuffer *object, shadow_params params);

// ideas to be implemented below...
// void gb_scroll_y(gbuffer *gb, int y_diff);
// void gb_copy_blurred();
// void gb_copy_resized();
// void gb_copy_masked();
// void gb_noise(gbuffer *gb);
// void gb_darken(gbuffer *gb, int radius);
// void gb_lighten(gbuffer *gb, int radius);
// void gb_line(gbuffer *gb, int radius);
// void gb_crop(gbuffer *gb, garea new_area);
// somehow i may have to make a mask... 
// we also *need* a clip rect for performance, in all operations! :-(


