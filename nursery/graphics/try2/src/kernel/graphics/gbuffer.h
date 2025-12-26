#pragma once
#include <stdint.h>
#include "../memory/malloc.h"
#include "../memory/string.h"
#include "color.h"
#include "fonts/font8x16.h"
#include "cursors/mouse_cursor.h"
#include "geometry.h"


typedef struct gbuffer {
    area area; // origin is always (0,0), to allow operations with nested buffers
    int buffer_size;
    uint32_t *buffer_argb;
} gbuffer;

typedef enum color_fill_type {
    FILL_TYPE_SOLID,
    FILL_TYPE_LINEAR_GRADIENT,
} color_fill_type;

typedef struct color_params {
    color_fill_type fill_type;
    color clr;
    color clr2;
    location gradient_p1;
    location gradient_p2;
    ease_function *ease;
    vector gradient_v;
    float gradient_len_sq;
} color_params;

static color_params color_params_solid(color clr) { return (color_params){.fill_type = FILL_TYPE_SOLID, .clr = clr}; };
static color_params color_params_gradient(color c1, color c2, location p1, location p2, ease_function ease) { return (color_params){
    .fill_type = FILL_TYPE_LINEAR_GRADIENT, .clr = c1, .clr2 = c2, .gradient_p1 = p1, .gradient_p2 = p2, .ease = ease,
    .gradient_v = vector_from_to(p1, p2), 
    .gradient_len_sq = vector_dot_product(vector_from_to(p1, p2), vector_from_to(p1, p2))
}; }

typedef struct shadow_params {
    color clr;
    uint8_t opacity;
    int offset_x;
    int offset_y;
    int blur_radius;
} shadow_params;

static shadow_params shadow_params_of(color clr, uint8_t opacity, int offset_x, int offset_y, int blur_radius) { return (shadow_params){.clr = clr, .opacity = opacity, .offset_x = offset_x, .offset_y = offset_y, .blur_radius = blur_radius}; }


void initialize_gbuffer(gbuffer *aux_buffer); // called from graphics initialization code


gbuffer *new_gbuffer(int width, int height);
void gb_free(gbuffer *gb);
void gb_clear(gbuffer *gb);
color gb_get_pixel(gbuffer *gb, location p);
void gb_paint_pixel(gbuffer *gb, location p, color clr);
void gb_fill(gbuffer *gb, color clr);
void gb_rect(gbuffer *gb, area rect, color_params cp, int radius);
void gb_border(gbuffer *gb, area rect, int radius, int border_width, color clr);
void gb_blur(gbuffer *gb, area rect, int radius, int do_blur_alpha);
int  gb_text(gbuffer *gb, const char *text, int x, int base_y, font8x16 *f, color clr);
void gb_text_demo(gbuffer *gb, int x, int base_y, font8x16 *f, color clr);
void gb_draw_cursor32_fast(gbuffer *gb, location mouse_pos, const cursor32 *cursor);

void gb_copy_area_fast(gbuffer *dest, gbuffer *src, size size, location dest_origin, location src_origin);
void gb_copy_area_with_alpha(gbuffer *dest, gbuffer *src, size size, location dest_origin, location src_origin, uint8_t global_alpha);
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


