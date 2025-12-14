#include <stdint.h>
#include "graphics.h"
#include "color.h"
#include "font5x7.h"
#include "../memory/string.h"


struct graphics_surface {
    int width;
    int height;
    uint32_t *pixels_array;  // each pixel colors RGBA color
    int opacity;
    int blur_radius;
    int shadow_radius;
};

struct graphics_global_info {
    uint8_t *fb_address;
    int      fb_width;
    int      fb_height;
    int      fb_pitch;
    int      fb_bpp;
};

struct graphics_global_info ggi;


void graphics_initialize(void *fb_address, int width, int height, int pitch, int bpp) {
    ggi.fb_address = fb_address;
    ggi.fb_width = width;
    ggi.fb_height = height;
    ggi.fb_pitch = pitch;
    ggi.fb_bpp = bpp;
}

void graphics_fill(color clr) {
    uint8_t red   = RGB_R(clr);
    uint8_t green = RGB_G(clr);
    uint8_t blue  = RGB_B(clr);

    for (int y = 0; y < ggi.fb_height; y++) {
        uint8_t *ptr = ((uint8_t *)ggi.fb_address) + y * ggi.fb_pitch;
        for (int x = 0; x < ggi.fb_width; x++) {
            *ptr++ = blue;
            *ptr++ = green;
            *ptr++ = red;
        }
    }
}

void graphics_rect(int x, int y, int width, int height, color clr) {
    uint8_t red   = RGB_R(clr);
    uint8_t green = RGB_G(clr);
    uint8_t blue  = RGB_B(clr);

    if (x + width > ggi.fb_width)
        width = ggi.fb_width - x;
    if (y + height > ggi.fb_height)
        height = ggi.fb_height - y;
    
    for (int y_offs = 0; y_offs < height; y_offs++) {
        uint8_t *ptr = ((uint8_t *)ggi.fb_address) + (y + y_offs) * ggi.fb_pitch + (x * 3);
        for (int x = 0; x < width; x++) {
            *ptr++ = blue;
            *ptr++ = green;
            *ptr++ = red;
        }
    }
}

static void inline graphics_pixel(int x, int y, color clr) {
    if (x >= ggi.fb_width) return;
    if (y >= ggi.fb_height) return;
    
    uint8_t *ptr = ggi.fb_address + y * ggi.fb_pitch + x * 3;
    *ptr++ = RGB_B(clr);
    *ptr++ = RGB_G(clr);
    *ptr++ = RGB_R(clr);
}

void graphics_demo(int left, int top, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            graphics_pixel(left + x, top + y, RGB(x, y, x^y));
        }
    }
}

int graphics_draw_character(int x, int baseline_y, char chr, color clr) {
    const glyph5x7 *gl = get_5x7_glyph(chr);

    for (int line = 0; line < 9; line++) {
        uint8_t bitmap = gl->bitmap[line];
        if (bitmap == 0)
            continue;

        for (int column = 0; column < gl->width; column++) {
            if ((bitmap & (0x80 >> column)) == 0)
                continue;
          
            graphics_pixel(x + column, baseline_y - 6 + line, clr);
        }
    }

    return gl->width;
}

int graphics_draw_text(int x, int baseline_y, const char *text, color clr) {
    int running_x = x;

    while (*text) {
        int width = graphics_draw_character(running_x, baseline_y, *text, clr);
        running_x += width + 1; // add space between the characters
        text++;
    }

    return running_x - x;
}



/*
gs *new_graphics_surface(int width, int height);

int gs_text(gs *g, int x, int y, const char *text, color color);
int gs_fill(gs *g, color color) {
    int len_uint32s = g->width * g->height;
    memset(g->pixels_array, color, len_uint32s * sizeof(uint32_t));
    return 0;
}

int gs_rect(gs *g, int x, int y, int width, int height, int corner_radius, color color);
int gs_frame(gs *g, int x, int y, int width, int height, int corner_radius, color color);
int gs_gradient_rect(gs *g, int x, int y, int width, int height, color center, float variance_factor, int direction);
int gs_copy_gs(gs *target, gs *sprite, int x, int y, int transparency, int blurryness, int shadow_distance, int shadow_radius);
*/