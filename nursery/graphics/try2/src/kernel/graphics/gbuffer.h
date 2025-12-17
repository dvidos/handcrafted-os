#pragma once
#include <stdint.h>
#include "../memory/malloc.h"
#include "../memory/string.h"
#include "color.h"
#include "font8x16.h"

typedef struct gbuffer {
    int height;
    int width;
    int buffer_size;
    uint32_t *buffer_argb;
} gbuffer;

typedef struct gpoint {
    int x;
    int y;
} gpoint;

typedef struct gsize {
    int width;
    int height;
} gsize;

typedef struct garea {
    gpoint origin;
    gsize size;
} garea;

inline gpoint gpoint_of(int x, int y) { return (gpoint){.x = x, .y = y}; }
inline gpoint gpoint_zero() { return (gpoint){.x = 0, .y = 0}; }
inline gsize gsize_of(int w, int h) { return (gsize){.width = w, .height = h}; }
inline garea garea_of(int x, int y, int w, int h) { return (garea){.origin = (gpoint){.x = x, .y = y}, .size = (gsize){.width = w, .height = h}}; }
inline gsize gb_size(gbuffer *gb) { return (gsize){.width = gb->width, .height = gb->height}; }

gbuffer *new_gbuffer(int width, int height);
void gb_free(gbuffer *gb);
void gb_set_pixel(gbuffer *gb, int x, int y, color clr);
color gb_get_pixel(gbuffer *gb, int x, int y);
void gb_fill(gbuffer *gb, color clr);
void gb_fill_rect(gbuffer *gb, int x, int y, int width, int height, color clr);
void gb_rect_border(gbuffer *gb, int x, int y, int width, int height, color clr);
int gb_text(gbuffer *gb, const char *text, int x, int base_y, font8x16 *f, color clr);
void gb_text_demo(gbuffer *gb, int x, int base_y, font8x16 *f, color clr);
void gb_copy_area(gbuffer *dest, gbuffer *src, gsize size, gpoint dest_origin, gpoint src_origin);


// note: for alpha, we need true 32 bit buffers and framebuffer.
// we must make our buffers true 32 bit, to support alpha, and then, when copying to the VBE, convert the alpha as we copy.
// that means we can copy whole uint32_t at a time.
// so... more changes!
void gb_copy_area_with_alpha(gbuffer *dest, gbuffer *src, gsize size, gpoint dest_origin, gpoint src_origin);
void gb_copy_area_scaled(gbuffer *gb, ...);
void gb_blur(gbuffer *gb, int radius);



// ideas to be implemented below...
void gb_scroll_y(gbuffer *gb, int y_diff);
void gb_copy_with_alpha();
void gb_copy_blurred();
void gb_copy_resized();
void gb_copy_masked();
void gb_blur(gbuffer *gb, int radius);
void gb_noise(gbuffer *gb);
void gb_darken(gbuffer *gb, int radius);
void gb_lighten(gbuffer *gb, int radius);
void gb_line(gbuffer *gb, int radius);
void gb_crop(gbuffer *gb, garea new_area);
// somehow i may have to make a mask...
void gb_rect_filled_rounded(gbuffer *dest, int radius);
void gb_rect_border_rounded(gbuffer *dest, int radius);
void gb_gradient_rect(gbuffer *dest);

