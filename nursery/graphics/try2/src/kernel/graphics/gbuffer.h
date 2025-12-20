#pragma once
#include <stdint.h>
#include "../memory/malloc.h"
#include "../memory/string.h"
#include "color.h"
#include "font8x16.h"
#include "ggeometry.h"


typedef struct gbuffer {
    // origin is always (0,0), to allow operations with nested buffers
    garea area;
    int buffer_size;
    uint32_t *buffer_argb;
} gbuffer;


void gb_set_aux_buffer(gbuffer *aux_buffer);

gbuffer *new_gbuffer(int width, int height);
void gb_free(gbuffer *gb);
void gb_set_pixel(gbuffer *gb, gpoint p, color clr);
color gb_get_pixel(gbuffer *gb, gpoint p);
void gb_fill(gbuffer *gb, color clr);
void gb_fill_rect(gbuffer *gb, garea rect, color clr);
void gb_fill_rect_rounded(gbuffer *gb, garea rect, int radius, color clr);
void gb_rect_border(gbuffer *gb, garea rect, color clr);
void gb_blur(gbuffer *gb, garea rect, int radius, int do_blur_alpha);
int  gb_text(gbuffer *gb, const char *text, int x, int base_y, font8x16 *f, color clr);
void gb_text_demo(gbuffer *gb, int x, int base_y, font8x16 *f, color clr);
void gb_copy_area(gbuffer *dest, gbuffer *src, gsize size, gpoint dest_origin, gpoint src_origin);
void gb_copy_area_with_alpha(gbuffer *dest, gbuffer *src, gsize size, gpoint dest_origin, gpoint src_origin, uint8_t global_alpha);
void gb_copy_area_scaled(gbuffer *gb, ...);

// next things:
// 1. gradients
// 2. improve blurring
// 3. border with rounded corner.

// and then:
// - draw off-white window with rounded corners, title bar with gradient fill, rounded border light gray, and shadow.

void gb_gradient_rect(gbuffer *gb, garea rect, gpoint g1, gpoint g2, color c1, color c2, ease_function ease);
void gb_rect_border_rounded(gbuffer *gb, garea rect, int radius, int border_width, color clr);



// ideas to be implemented below...
void gb_scroll_y(gbuffer *gb, int y_diff);
void gb_copy_blurred();
void gb_copy_resized();
void gb_copy_masked();
void gb_noise(gbuffer *gb);
void gb_darken(gbuffer *gb, int radius);
void gb_lighten(gbuffer *gb, int radius);
void gb_line(gbuffer *gb, int radius);
void gb_crop(gbuffer *gb, garea new_area);
// somehow i may have to make a mask...


