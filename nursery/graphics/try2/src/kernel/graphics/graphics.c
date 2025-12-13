#include <stdint.h>
#include "graphics.h"
#include "color.h"
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

void graphics_fill(color c) {
    uint8_t red   = (c >> 16) & 0xFF;
    uint8_t green = (c >> 8)  & 0xFF;
    uint8_t blue  = (c >> 0)  & 0xFF;

    for (int y = 0; y < ggi.fb_height; y++) {
        for (int x = 0; x < ggi.fb_width; x++) {
            uint8_t *pix_start = ((uint8_t *)ggi.fb_address) + y * ggi.fb_pitch + x * 3;
            pix_start[2] = red;
            pix_start[1] = green;
            pix_start[0] = blue;
        }
    }
}

void graphics_demo(int left, int top, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t *pix_start = ggi.fb_address + (top + y) * ggi.fb_pitch + (left + x) * 3;
            pix_start[0] = y & 0xFF; // blue
            pix_start[1] = x & 0xFF; // green
            pix_start[2] = (y^x) & 0xFF; // red
        }
    }
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